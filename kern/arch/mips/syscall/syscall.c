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

off_t lseek(int fd, off_t pos, int whence, int32_t *retval){
	struct fileContainer *file = curproc->fileTable[fd];
	struct stat *statBox = kmalloc(sizeof(*statBox));
	
	if(file == NULL){
		*retval = -1;
		return EBADF;
	}
	
	VOP_STAT(file->llfile, statBox); //This is where the NULL dereference is happening
	if(whence == SEEK_SET){
		file->offset = pos;
	}
	else if(whence == SEEK_CUR){
		file->offset = (file->offset) + (pos);

	}
	else if(whence == SEEK_END){
		//kprintf("Offset, pos: %d, %d \n", (int)file->offset, (int)pos);
		file->offset = (statBox->st_size + pos);
	}
	else{
		//whence is invalid
		*retval = -1;
		return EINVAL;
	}
	*retval = file->offset; 
	return (off_t)0;

	
}

ssize_t open(char *filename, int flags, int32_t *retval){
	//TODO
	//Search filetable for first empty slot
	//FIX THIS ITS SHIT
	//You can change function prototypes, pass int value into all sys calls
	struct fileContainer *file;
	struct vnode *trash;
	trash = kmalloc(sizeof(struct vnode));
	file = kmalloc(sizeof(struct fileContainer));
	file->lock = lock_create("mememachine");
	lock_acquire(file->lock);
	file->refCount = kmalloc(sizeof(int));
	*file->refCount = 1;
	char* filestar = filename;
	vfs_open(filestar, flags, 0, &trash);
	file->llfile = trash;
	file->permflag = flags;
	file->offset = 0;
	

	for(int i = 0; i < 64; i++){
		if(curproc->fileTable[i]== NULL){
			curproc->fileTable[i] = file;
			*retval = (int32_t)i;
			break;
		}
	}

	lock_release(file->lock);
	return (ssize_t)0;
}

ssize_t write(int filehandle, const void *buf, size_t size, int32_t *retval){
	//kprintf("-");
	// if(filehandle == 2){
	// 	*retval = (int32_t)size;
	// 	return (ssize_t)0;

	// }
	if(curproc->fileTable == NULL){
		kprintf("get fetched");
	}

	struct fileContainer *file = curproc->fileTable[filehandle];

	if (file == NULL || file->permflag == O_RDONLY){
		*retval = (int32_t)0;
		return EBADF;
	}
	KASSERT(file->lock != NULL);
	lock_acquire(file->lock);
	//kprintf("%p\n", &file->lock);
	//kprintf("Refcount: %d\n", *file->refCount);
	//Remember to give the user the size variable back (copyout, look at sys__time)
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

	VOP_WRITE(file->llfile, &thing);

	file->offset = file->offset + size;

	*retval = (int32_t)size;
	KASSERT(file->lock != NULL);

	lock_release(file->lock);
	//kprintf("lock released\n");

	return (ssize_t)0;
}

ssize_t close(int fd, int32_t *retval){
	
	if(fd < 0 || fd > 63 || curproc->fileTable[fd] == NULL){

		*retval = 0;
		return (ssize_t)0;
	}
	//kprintf("we close now\n");
	lock_acquire(curproc->fileTable[fd]->lock);
	*curproc->fileTable[fd]->refCount = *curproc->fileTable[fd]->refCount - 1;
	lock_release(curproc->fileTable[fd]->lock);

	if(*curproc->fileTable[fd]->refCount == 0){
		kfree(curproc->fileTable[fd]->llfile);
		kfree(curproc->fileTable[fd]);
	}

	curproc->fileTable[fd] = NULL;
	*retval = (int32_t)0;
	return (ssize_t)0;
}

ssize_t read(int fd, void *buf, size_t buflen, int32_t *retval){
	struct fileContainer *file = curproc->fileTable[fd];
	if (file == NULL || file->permflag == O_WRONLY){
		return EBADF;
	}
	lock_acquire(file->lock);

	//Remember to give the user the size variable back (copyout, look at sys__time)
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

	VOP_READ(file->llfile, &thing);

	file->offset = file->offset + buflen;
	*retval = (int32_t)buflen;
	lock_release(file->lock);

	return (ssize_t)0;
}

//copy the trapframe
//fork the thread
//the thread's function is a helper function
//set the parent's return value to be the child's pid
//parent returns like any other syscall
//pass trapframe into mips_usermode
//helper function has to associate the parent and the child
//set the trapframe's proper return values
//child function copies

//REMEMBER CHILDREN
//THE PROCESS TABLE BELONGS TO KPROC
//THE HIGHEST PID BELONGS TO KPROC
//THE MASTER LOCK BELONGS TO KPROC
//YOUR SOUL BELONGS TO KPROC
pid_t fork(struct trapframe *tf, int32_t *retval){
	(void) retval;
	lock_acquire(kproc->proc_lock);
	//Initialize pointer to new process
	struct proc *newProc;
	newProc = kmalloc(sizeof(struct proc));
	//Initialize pointer to a new file table for the new process
	struct fileContainer* *newFT;
	newFT = kmalloc(64*sizeof(struct fileContainer*));

	//Copy curproc's file table to new process
	newProc->fileTable = newFT;
	for(int i = 0; i < 64; i++){
		if(curproc->fileTable[i] != NULL){
			newProc->fileTable[i] = curproc->fileTable[i];
			*newProc->fileTable[i]->refCount = *newProc->fileTable[i]->refCount + 1;
		}
	}
 
	//Copy lots of other stuff to new process
	as_copy(curproc->p_addrspace, &newProc->p_addrspace);
	newProc->p_cwd = curproc->p_cwd;
	newProc->p_numthreads = curproc->p_numthreads;
	newProc->p_name = curproc->p_name;

	//Search for first empty place in process table starting at highestpid, place proc in it
	//Also does wraparound
	for(int i = *kproc->highestPid - 1; i < 2000; i++){
		if(kproc->procTable[i] == NULL){
			kproc->procTable[i] = newProc;
			newProc->pid = i + 1;
			*kproc->highestPid = newProc->pid + 1;
			if (*kproc->highestPid > 2000){
				*kproc->highestPid = 1;
			}
			break;
		}
		//If you make it to the last index, wrap around via highestpid
		i = (i == 1999)? 0 : i;
	}
	
	//Set some specifics for the new process, including the parent's pid (curproc)
	newProc->exitCode = -1;
	newProc->waitingOnMe = 0;
	newProc->parentpid = curproc->pid;
	newProc->proc_lock = lock_create("proclockelse");


	lock_release(kproc->proc_lock);

	kprintf("Refcount on curproc: %d\n", *curproc->fileTable[0]->refCount);
	//Fork new process
	thread_fork(newProc->p_name, newProc, copytf, tf, 0);

	//Return child's pid to userland
	*retval = (int32_t)newProc->pid;
	//Return errno 
	return (pid_t)0;
}

void copytf(void *tf, unsigned long ts){
	(void)ts;

	// while(1){
	// 	//Plugs the child thread so it can't go dicking around
	// }
	//Activitates new address space
	as_activate();
	lock_acquire(curproc->proc_lock);
	//Copies parent trapframe (ptf) to child trapframe (ctf)
	struct trapframe *ptf = (struct trapframe *)tf;
	struct trapframe ctf = *ptf;
	//Gives ctf the right return values (returns 0)
	ctf.tf_v0 = 0;
	ctf.tf_v1 = 0;
	ctf.tf_a3 = 0;
	//Moves child thread to next instruction so it doesn't fork for fucking ever
	ctf.tf_epc += 4;
	//Give the child trapframe to usermode. Say bye bye.
	curproc->exitCode = 0;
	_exit(0);

	//mips_usermode(&ctf);

}

//IF WE HAVE MEMORY PROBLEMS, LOOKY HERE
//We can save on memory if we only have one condition variable
//But will be more inefficient in terms of the broadcast
pid_t waitpid(pid_t pid, int *status, int options, int32_t *retval){
	(void)pid;
	(void)status;
	(void)options;
	(void)retval;
	//kprintf("Ssdssdssdgihbzj\n");
	struct proc* procToReap = kproc->procTable[pid+1];

	//If child process does not exist, waitpid fails
	if(procToReap == NULL){
		//WAITPID FAILS HERE
		*retval = -1;
		return (pid_t)ESRCH;
	}
	//If the child has not yet exited, sleep the parent using the child's lock and cv
	if(procToReap->exitCode == -1){
		lock_acquire(procToReap->proc_lock);
		lock_release(procToReap->proc_lock);
	}
	
	
	//The child should have exited if you get here. Reap it.
	if(procToReap->exitCode != -1){
		lock_acquire(kproc->proc_lock);
		//Free up other shit
		if (procToReap->p_cwd) {
			VOP_DECREF(procToReap->p_cwd);
			procToReap->p_cwd = NULL;
		}
		//lock_destroy(procToReap->proc_lock);
		// for(int i = 0; i < 2000; i++){
		// 	if(procToReap->fileTable[i] != NULL && *procToReap->fileTable[i]->refCount == 1){
		// 		kfree(procToReap->fileTable[i]->llfile);
		// 		kfree(procToReap->fileTable[i]->refCount);
		// 		kfree(procToReap->fileTable[i]->lock);
		// 		kfree(procToReap->fileTable[i]);
		// 	}
		// 	procToReap->fileTable[i] = NULL;
		// }
		
		//spinlock_cleanup(&procToReap->p_lock);

		
		
		kfree(procToReap->fileTable);
		kproc->procTable[pid+1] = NULL;
		if (procToReap->p_addrspace) {
		
			struct addrspace *as;

			if (procToReap == curproc) {
				as = proc_setas(NULL);
				as_deactivate();
			}
			else {
				as = procToReap->p_addrspace;
				procToReap->p_addrspace = NULL;
			}
			as_destroy(as);
		}
		*retval = 0;
		lock_release(kproc->proc_lock);
		return (pid_t)0;
	}
	
	//kprintf("asdewrwqer\n");
	return (pid_t)0;
}

// void freeFileContainer(struct fileContainer *fileCont){
// 	kfree(fileCont->llfile);
// 	kfree(fileCont->refCount);
// 	kfree(fileCont->lock);
// 	kfree(fileCont);
// 	return;
// }

void getpid(int32_t *retval){
	*retval = (int32_t)curproc->pid;
}

void _exit(int exitcode){

	if (curproc->p_cwd) {
		VOP_DECREF(curproc->p_cwd);
		curproc->p_cwd = NULL;
	}

	if (curproc->p_addrspace) {

		struct addrspace *as;
		
		as = proc_setas(NULL);
		as_deactivate();

		as_destroy(as);
	}
	//spinlock_cleanup(&curproc->p_lock);

	curproc->exitCode = exitcode;	
	//kprintf("WE HIT EXIT\n");
	//kprintf("Pid of exiter: %d", curproc->pid);
	if(lock_do_i_hold(curproc->proc_lock)){
		lock_release(curproc->proc_lock);
		
	}
	thread_exit();
}

int execv(const char *program, char **args, int32_t *retval){
	(void)program;
	(void)args;
	(void)retval;

	return 0;
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
	// struct fileContainer* f1 = kmalloc(sizeof(f1));
	// struct fileContainer* f2 = kmalloc(sizeof(f2));
	// f1->offset = 0;
	// f1->permflag = O_WRONLY;
	// f1->llfile = curproc->fileTable[1]->llfile;
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
		
		// cloneFileContainer(f1, f2);
		// //kfree(f1);
		// //f1 = NULL;
		// kprintf("%p\n%p", (void*)&f1->offset, (void*)&f2->offset);
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
		//kprintf("lseek retval: %d\n", retval);
		break;

		case SYS_fork:
		err = fork(tf,&retval);
		// while(1){

		// }
		//kprintf("Retval in switch: %d\n", retval);
		
		//kprintf("Do we even get here? pid: %d \n", curproc->pid);
		break;

		case SYS_getpid:
		err = 0;
		getpid(&retval);
		break;

		case SYS_waitpid:
		err = waitpid(tf->tf_a0, (int*)tf->tf_a1, tf->tf_a2, &retval);
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
		//kprintf("Error: %d\n", err);
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
		//kprintf("Retval in tf: %d\n", retval);
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

/*
 * Enter user mode for a newly forked process.
 *
 * This function is provided as a reminder. You need to write
 * both it and the code that calls it.
 *
 * Thus, you can trash it and do things another way if you prefer.
 */
void
enter_forked_process(struct trapframe *tf)
{
	(void)tf;
}
