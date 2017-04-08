/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <signal.h>
#include <syscall.h>
#include <uio.h>
#include <vnode.h>
#include <vfs.h>
#include <proc.h>
#include <kern/fcntl.h>
#include <kern/seek.h>
#include <copyinout.h>
#include <kern/stat.h>
#include <addrspace.h>
#include <synch.h>
#include <limits.h>
#include <kern/wait.h>
/*
 * System call dispatcher.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception-*.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. 64-bit arguments are passed in *aligned*
 * pairs of registers, that is, either a0/a1 or a2/a3. This means that
 * if the first argument is 32-bit and the second is 64-bit, a1 is
 * unused.
 *
 * This much is the same as the calling conventions for ordinary
 * function calls. In addition, the system call number is passed in
 * the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, or v0 and v1 if 64-bit. This is also like an ordinary
 * function call, and additionally the a3 register is also set to 0 to
 * indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/user/lib/libc/arch/mips/syscalls-mips.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * If you run out of registers (which happens quickly with 64-bit
 * values) further arguments must be fetched from the user-level
 * stack, starting at sp+16 to skip over the slots for the
 * registerized values, with copyin().
 */
struct lock* procLock;
int	highPid;
struct proc* procTable[2000];

ssize_t open(char *filename, int flags, int32_t *retval){
	if(flags != 22 && flags != 21 && flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR && flags != O_CREAT && flags != O_EXCL && flags != O_TRUNC && flags != O_APPEND){
		*retval = (int32_t)0;
		return EINVAL;
	}

	char* ptr;
	int err = copyin((const_userptr_t)filename, &ptr, 4);
	if(err){
		*retval = (int32_t)0;
		return EFAULT;
	}

	//Build our fileContainer
	struct fileContainer *file = kmalloc(sizeof(*file));

	file->lock = lock_create("mememachine");
	lock_acquire(file->lock);
	
	file->refCount = kmalloc(sizeof(int));
	*file->refCount = 1;
	file->permflag = flags;
	file->offset = 0;
	
	char* filestar = filename;
	//Generate our vnode
	err = vfs_open(filestar, flags, 0, &file->llfile);
	if(err){
		fileContainerDestroy(file);
		*retval = 0;
		return err;
	}

	//Places our file in the first empty slot in curpro's fileTable
	//The user gets back the index at which it was placed (file descriptor)
	bool full = true;
	for(int i = 0; i < 64; i++){
		if(curproc->fileTable[i]== NULL){
			curproc->fileTable[i] = file;
			*retval = (int32_t)i;
			full = false;
			break;
		}
	}
	if(full){
		*retval = (int32_t)0;
		return ENOSPC;
	}
	lock_release(file->lock);

	return (ssize_t)0;
}

ssize_t close(int fd, int32_t *retval){
	//fd is not a valid file descriptor.
	if(fd < 0 || fd > 63 || curproc->fileTable[fd] == NULL){
		*retval = 0;
		return (ssize_t)EBADF;
	}
	//Decrement the reference count on the fileContainer being closed
	lock_acquire(curproc->fileTable[fd]->lock);
	*curproc->fileTable[fd]->refCount -= 1;

	//If no more references to this fileContainer exist, OBLITERATE IT
	if(*curproc->fileTable[fd]->refCount == 0){
		vfs_close(curproc->fileTable[fd]->llfile);
		lock_release(curproc->fileTable[fd]->lock);
		fileContainerDestroy(curproc->fileTable[fd]);
	}else{
		lock_release(curproc->fileTable[fd]->lock);
	}

	//Empty its space in the table.
	curproc->fileTable[fd] = NULL;
	if(retval != NULL){
		*retval = (int32_t)0;
	}

	return (ssize_t)0;
}

ssize_t read(int fd, void *buf, size_t buflen, int32_t *retval){
	
	char* ptr;
	int err = copyin((const_userptr_t)buf, &ptr, 4);
	if(err){
		*retval = (int32_t)0;
		return EFAULT;
	}

	if(fd < 0 || fd > 63){
		*retval = (int32_t)0;
		return EBADF;
	}
	
	//Fetch our fileContainer
	struct fileContainer *file = curproc->fileTable[fd];
	//fd is not a valid file descriptor, or was not opened for reading.
	if (file == NULL || file->permflag == O_WRONLY){
		return EBADF;
	}

	lock_acquire(file->lock);
	//Remember to give the user the size variable back (copyout, look at sys__time)
	//Builds all info we need to read
	struct uio thing;
	struct iovec iov;
	iov.iov_ubase = (userptr_t)buf;
	iov.iov_len = buflen;		 // length of the memory space
	thing.uio_iov = &iov;
	thing.uio_iovcnt = 1;
	thing.uio_resid = buflen; 
	thing.uio_offset = file->offset;
	thing.uio_segflg = UIO_USERSPACE;
	thing.uio_rw = UIO_READ;
	thing.uio_space = proc_getas();

	//Does the actual reading
	err = VOP_READ(file->llfile, &thing);
	if(err){
		*retval = -1;
		return err;
	}
	int amountRead = thing.uio_offset - file->offset;

	//The current seek position of the file is advanced by the number of bytes read.
	file->offset = file->offset + amountRead;
	//The count of bytes read is returned.
	*retval = (int32_t)amountRead;
	
	lock_release(file->lock);
	return (ssize_t)0;
}

ssize_t write(int filehandle, const void *buf, size_t size, int32_t *retval){
	
	if(buf == NULL){
		*retval = 0;
		return EFAULT;
	}

	if(buf == (void*)0x80000000 || buf == (void*)0x40000000){
		*retval = (int32_t)0;
		return EFAULT;
	}
	
	if(filehandle < 0 || filehandle > 63){
		*retval = (int32_t)0;
		return EBADF;
	}
	
	struct fileContainer *file = curproc->fileTable[filehandle];

	//fd is not a valid file descriptor, or was not opened for writing.	
	if (file == NULL || file->permflag == O_RDONLY || file->permflag == 4){
		*retval = (int32_t)0;
		return EBADF;
	}
	

	KASSERT(file->lock != NULL);
	lock_acquire(file->lock);

	//Builds all the info we need to write
	struct uio thing;
	struct iovec iov;
	iov.iov_ubase = (userptr_t)buf;
	iov.iov_len = size;		 // length of the memory space
	thing.uio_iov = &iov;
	thing.uio_iovcnt = 1;
	thing.uio_resid = size; 
	thing.uio_offset = file->offset;
	thing.uio_segflg = UIO_USERSPACE;
	thing.uio_rw = UIO_WRITE;
	thing.uio_space = proc_getas();
	//Does the actual writing
	VOP_WRITE(file->llfile, &thing);
	//The current seek position of the file is advanced by the number of bytes written.
	file->offset = file->offset + size;

	*retval = (int32_t)size;
	KASSERT(file->lock != NULL);
	lock_release(file->lock);
	return (ssize_t)0;
}

off_t lseek(int fd, off_t pos, int whence, int32_t *retval){
	

	if(fd < 0 || fd > 63 || curproc->fileTable[fd] == NULL){

		*retval = 0;
		return (ssize_t)EBADF;
	}
	
	struct fileContainer *file = curproc->fileTable[fd];
	struct stat statBox;

	//fd is not a valid file descriptor
	if(file == NULL){
		*retval = -1;
		return EBADF;
	}
	lock_acquire(file->lock);
	//Gets us the size of the file, stores it in statBox->st_size
	VOP_STAT(file->llfile, &statBox);
	if((int)(&statBox)->st_rdev){
		*retval = -1;
		lock_release(file->lock);
		return (ssize_t)ESPIPE;
	}

	if(whence == SEEK_SET){
		if(pos < 0){
			*retval = -1;
			lock_release(file->lock);

			return EINVAL;
		}
		file->offset = pos;
	}
	else if(whence == SEEK_CUR){
		if((file->offset) + (pos) < 0){
			*retval = -1;
			lock_release(file->lock);

			return EINVAL;
		}
		file->offset = (file->offset) + (pos);

	}
	else if(whence == SEEK_END){
		if(((&statBox)->st_size + pos) < 0){
			*retval = -1;
			lock_release(file->lock);

			return EINVAL;
		}
		file->offset = ((&statBox)->st_size + pos);
	}
	else{
		//whence is invalid
		*retval = -1;
		lock_release(file->lock);

		return EINVAL;
	}
	*retval = file->offset; 
	lock_release(file->lock);

	return (off_t)0;
}

int dup2(int oldfd, int newfd, int32_t *retval){
	//If newfd names an already-open file, that file is closed.
	if(oldfd == newfd){
		*retval = newfd;
		return 0;
	}
	if(oldfd < 0 || oldfd > 63){
		*retval = (int32_t)0;
		return EBADF;
	}
	if(newfd < 0 || newfd > 63){
		*retval = (int32_t)0;
		return EBADF;
	}
	if(curproc->fileTable[oldfd] == NULL ){
		*retval = (int32_t)0;
		return EBADF;
	}
	lock_acquire(curproc->fileTable[oldfd]->lock);
	if(curproc->fileTable[newfd] != NULL ){
		close(newfd, 0);
	}
	/*Clones the file handle identifed by file descriptor oldfd onto the file handle
	 identified by newfd. The two handles refer to the same "open" of the file -- that 
	 is, they are references to the same object and share the same seek pointer.*/
	curproc->fileTable[newfd] = curproc->fileTable[oldfd];
	*curproc->fileTable[newfd]->refCount += 1;
	lock_release(curproc->fileTable[oldfd]->lock);
	*retval = newfd;
	return 0;
}

pid_t fork(struct trapframe *tf, int32_t *retval){

	//Initialize pointer to new process
	struct proc *newProc;
	newProc = proc_create_runprogram("child");
	//I don't know why newProc would ever be NULL but if we remove this, forkbomb fails.
	if(newProc == NULL){
		*retval = 0;
		return 0;

	}
	newProc->proc_lock = lock_create("proclockelse");
	
	// Copy curproc's file table to new process
	for(int i = 0; i < 64; i++){
		if(curproc->fileTable[i] != NULL){
			lock_acquire(curproc->fileTable[i]->lock);
			newProc->fileTable[i] = curproc->fileTable[i];
			//All shared files get their reference count incremented
			*newProc->fileTable[i]->refCount += 1;
			lock_release(curproc->fileTable[i]->lock);
		}
	}
	
	//Copy address space and cwd
	int res = as_copy(curproc->p_addrspace, &newProc->p_addrspace);
	//This isn't need to pass, but removing it produces...interesting...results in forkbomb
	if(res == 3){
		*retval = 0;
		return 0;
	}
	newProc->p_cwd = curproc->p_cwd;
	
	/*Search for first empty place in process table starting at highestpid, 
	place proc in it using wraparound*/
	lock_acquire(procLock);
	for(int i = highPid - 1; i < 2000; i++){
		if(procTable[i] == NULL){
			procTable[i] = newProc;
			newProc->pid = i + 1;
			highPid++;
			if (highPid > 2000){
				highPid = 2;
			}
			break;
		}
		//If you make it to the last index, wrap around via highestpid
		i = (i == 1999)? 2 : i;
	}
	lock_release(procLock);

	//Set some specifics for the new process, including the parent's pid (curproc)
	newProc->exitCode = -1;
	newProc->parentpid = curproc->pid;
	newProc->p_numthreads = 0;
	//Deep copy trapframe
	struct trapframe *tfc = kmalloc(sizeof(struct trapframe));
	//I don't know why this would be NULL either but removing it breaks forkbomb
	if(tfc == NULL){
		*retval = 0;
		return 0;
	}
	*tfc = *tf;
	//Fork new process
	thread_fork(newProc->p_name, newProc, copytf, tfc, 0);
	//Return child's pid to userland
	*retval = (int32_t)newProc->pid;
	//Return errno 
	return (pid_t)0;
}

void copytf(void *tf, unsigned long ts){
	(void)ts;
	//Activitates new address space
	as_activate();
	lock_acquire(curproc->proc_lock);
	//Copies parent trapframe (ptf) to child trapframe (ctf)
	struct trapframe ctf = *(struct trapframe *)tf;
	kfree(tf);
	//Gives ctf its return values (returns 0)
	ctf.tf_v0 = 0;
	ctf.tf_v1 = 0;
	ctf.tf_a3 = 0;
	//Moves child thread to next instruction so it doesn't fork for-fucking-ever
	ctf.tf_epc += 4;
	//Ship the child trapframe to usermode. Wave goodbye.
	curproc->exitCode = -1;
	mips_usermode(&ctf);
}

void getpid(int32_t *retval){

  *retval = (int32_t)curproc->pid;
/*  	     ___^___ _
	 L    __/      [] \
	LOL===__           \
	 L      \___ ___ ___]
	              I   I
	          ----------/ 
*/
}

pid_t waitpid(pid_t pid, int *status, int options, int32_t *retval){
	if(options != 0){
		*retval = -1;
		return EINVAL;
	}
	char* ptr;
	int err = copyin((const_userptr_t)status, &ptr, 4);
	if(err && (status != NULL)){
		*retval = (int32_t)0;
		return EFAULT;

	}
	//Fetch the process we are waiting on to drop dead
	struct proc* procToReap = procTable[pid-1];

	//ERR: The pid argument named a nonexistent process.
	if(procToReap == NULL){
		*retval = -1;
		return (pid_t)ESRCH;
	}
	if(procToReap->parentpid != curproc->pid){
		*retval = -1;
		return ECHILD;
	}
	if(curproc->parentpid == procToReap->parentpid){
		*retval = -1;
		return ECHILD;
	}
	//If the child has not yet exited, sleep the parent
	while(procToReap->p_numthreads > 0){
		
		lock_acquire(procToReap->proc_lock);
		lock_release(procToReap->proc_lock);
	}
	
	lock_acquire(procToReap->proc_lock);
	//Copies the exitcode out to userland through status
	if(procToReap->signal){
		int temp = _MKWAIT_CORE(procToReap->exitCode);
		copyout(&temp, (userptr_t)status, 4);

	}else{
		int temp = _MKWAIT_EXIT(procToReap->exitCode);
		copyout(&temp, (userptr_t)status, 4);
	}
	
	lock_release(procToReap->proc_lock);

	lock_acquire(procLock);
	procTable[procToReap->pid - 1] = NULL;
	lock_release(procLock);

	*retval = procToReap->pid;
	proc_destroy(procToReap);

	return (pid_t)0;
}

void sys_exit(int exitcode,bool signaled){
	for(int i = 0; i < 64; ++i){
		int ret = 0;
		if(!(curproc->fileTable[i] == NULL)){
			close(i,&ret);
		}
	}

	if(lock_do_i_hold(curproc->proc_lock)){
		lock_release(curproc->proc_lock);
	}

	curproc->exitCode = exitcode;
	curproc->signal = signaled;
	thread_exit();
}

int rounded(int a){
	if(a%4 == 0){
		return a;
	}
	return rounded(a+1);
}

int execv(const char *program, char **args, int32_t *retval){
	
	int fakeptr;
	int err1 = copyin((const_userptr_t)program, &fakeptr, 4);
	if (err1){
		return err1;
	}
	int err2 = copyin((const_userptr_t)args, &fakeptr, 4);
	if (err2){
		return err2;
	}
	int err3 = copyin((const_userptr_t)*args, &fakeptr, 4);
	if (err3){
		return err3;
	}

	if(program == NULL  ||args == NULL || (void*)args == (void*)0x80000000 || (void*)args == (void*)0x40000000 || (void*)program == (void*)0x80000000 || (void*)program == (void*)0x40000000 || strlen(program) == 0){
		*retval = ENOENT;
		_exit(107);
	}

	int totalSize = 0;
	
	char * name = kmalloc(strlen(program)+1);
	int err = copyin((const_userptr_t)program, name , rounded(strlen(program)+1));
	
	if(err != 0){
		_exit(107);
	}

	int nargs = 0;
	int sizeOfLastArgs = 0;

	for(int i = 0; i < __ARG_MAX/8; i++){
		if(*(args+i) == (void *)0x40000000){
			_exit(107);
		}
		if(*(args+i) == NULL){
			nargs++;
			break;
		}
		nargs++;
	}
	
	for(int i = 0; i < (nargs - 1); i++){
		totalSize += rounded(strlen(*(args + i)) + 1);
	}
	
	void * buffer = kmalloc((4*nargs) + totalSize);
	memset(buffer, '\0', (4*nargs) + totalSize);
	
	for(int i = 0; i < __ARG_MAX/8; i++){
		if(*(args+i) == NULL){
			int err = copyin((const_userptr_t)args, buffer + i , 4);
			if(err){
				_exit(107);
			}
			break;
		}
		copyin((const_userptr_t)args, buffer + i , 4);
	}
	
	for(int i = 0; i < (nargs - 1); i++){
		int err = copyin((const_userptr_t)*(args + i), (char *)(buffer + (4*nargs) + sizeOfLastArgs), rounded(strlen(*(args + i)) + 1));
		if(err){
			_exit(107);
		}
		sizeOfLastArgs += rounded(strlen(*(args + i)) + 1);
	}
	

	struct addrspace *as;
	as = proc_setas(NULL);
	as_deactivate();
	as_destroy(as);

	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	(void)args;
	(void)retval;
	/* Open the file. */
	result = vfs_open((char*)name, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	proc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

	// + 12 will be the size of programname
	stackptr -= ((4*nargs) + sizeOfLastArgs);
	sizeOfLastArgs = 0; 

	for(int i = 0; i < nargs; ++i){
		copyout(buffer + i, (userptr_t)stackptr, 4);
		stackptr += 4;
	}
	//housekeeping
	stackptr -= (4 * nargs);


	//reset size
	sizeOfLastArgs = 4*nargs;


	for(int i = 0; i < nargs; ++i){
		copyout((char*)(buffer + sizeOfLastArgs), (userptr_t)(stackptr + sizeOfLastArgs), rounded(strlen((char*)(buffer + sizeOfLastArgs)) + 1));
		*(char **)(stackptr + (4*i)) =  (char*)(stackptr +sizeOfLastArgs);
		sizeOfLastArgs += rounded(strlen((char*)(buffer + sizeOfLastArgs)) + 1);
	}
	nargs--;
	*(char **)(stackptr + (4*(nargs))) =  NULL;
	kfree(buffer);
	kfree(name);

	/* Warp to user mode. */
	enter_new_process(nargs/*argc*/, (userptr_t)stackptr/*userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
	stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

void
syscall(struct trapframe *tf)
{
	int callno;
	int32_t retval; //Passed to user
	int err; //Returned to kernel

	KASSERT(curthread != NULL);
	KASSERT(curthread->t_curspl == 0);
	KASSERT(curthread->t_iplhigh_count == 0);

	callno = tf->tf_v0;

	/*
	 * Initialize retval to 0. Many of the system calls don't
	 * really return a value, just 0 for success and -1 on
	 * error. Since retval is the value returned on success,
	 * initialize it to 0 by default; thus it's not necessary to
	 * deal with it except for calls that return other values,
	 * like write.
	 */

	retval = 0;
	off_t pos64;
	int whenceIn;
	switch (callno) {

		case SYS_reboot:
		err = sys_reboot(tf->tf_a0);
		break;

		case SYS___time:
		err = sys___time((userptr_t)tf->tf_a0, (userptr_t)tf->tf_a1);
		break;

		case SYS_open:
		err = open((char*)tf->tf_a0, (int)tf->tf_a1, &retval);
		break;

		case SYS_write:
		err = write(tf->tf_a0, (userptr_t)tf->tf_a1, tf->tf_a2, &retval);
		break;

		case SYS_close:
		err = close(tf->tf_a0, &retval);
		break;

		case SYS_read:
		err = read(tf->tf_a0, (userptr_t)tf->tf_a1, tf->tf_a2, &retval);
		break;

		case SYS_lseek:
		pos64 = tf->tf_a2;
		pos64 = pos64 << 32;
		pos64 = pos64 | tf->tf_a3;
		copyin((userptr_t)tf->tf_sp+16, &whenceIn, 4);
		pos64 = lseek(tf->tf_a0, pos64, whenceIn, &retval);
		err = (int)pos64;
		break;

		case SYS_dup2:
		err = dup2(tf->tf_a0, tf->tf_a1, &retval);
		break;

		case SYS_fork:
		err = fork(tf,&retval);
		break;

		case SYS_getpid:
		err = 0;
		getpid(&retval);
		break;

		case SYS_waitpid:
		err = waitpid(tf->tf_a0, (int *)tf->tf_a1, tf->tf_a2, &retval);
		break;

		case SYS_execv:
		err = execv((const char*)tf->tf_a0, (char**)tf->tf_a1, &retval);
		break;
		
		case SYS__exit:
		err = 0;
		_exit(tf->tf_a0);
		break;

		default:
		kprintf("Unknown syscall %d\n", callno);
		err = ENOSYS;
		break;
	}


	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		tf->tf_v0 = err;
		tf->tf_a3 = 1;      /* signal an error */
	}
	else {
		/* Success. */
		if(callno == SYS_lseek /*|| callno == SYS_fork*/){
		tf->tf_v0 = 0;
		tf->tf_v1 = retval;
		tf->tf_a3 = 0;  
	}
	else{
		tf->tf_v0 = retval;
		tf->tf_a3 = 0;      /* signal no error */
	}
}

	/*
	 * Now, advance the program counter, to avoid restarting
	 * the syscall over and over again.
	 */

tf->tf_epc += 4;

	/* Make sure the syscall code didn't forget to lower spl */
KASSERT(curthread->t_curspl == 0);
	/* ...or leak any spinlocks */
KASSERT(curthread->t_iplhigh_count == 0);
}