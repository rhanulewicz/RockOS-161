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

#ifndef _SYSCALL_H_
#define _SYSCALL_H_
#define _exit(x) sys_exit((x),false)
#define sig_exit(x) sys_exit((x),true)


#include <cdefs.h> /* for __DEAD */

extern struct proc* procTable[2000];
extern struct lock* procLock;
extern int highPid;



struct trapframe; /* from <machine/trapframe.h> */

/*
 * The system call dispatcher.
 */

void syscall(struct trapframe *tf);

/*
 * Support functions.
 */

/* Enter user mode. Does not return. */
__DEAD void enter_new_process(int argc, userptr_t argv, userptr_t env,
		       vaddr_t stackptr, vaddr_t entrypoint);


/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

ssize_t write(int filehandle, const void *buf, size_t size, int32_t *retval);
ssize_t open(char *filename, int flags,  int32_t *retval);
ssize_t close(int fd, int32_t *retval);
ssize_t read(int fd, void *buf, size_t buflen, int32_t *retval);
off_t lseek(int fd, off_t pos, int whence, int32_t *retval);
pid_t fork(struct trapframe *tf, int32_t *retval);
pid_t waitpid(pid_t pid, int *status, int options, int32_t *retval);
void sys_exit(int exitcode,bool signaled);
int execv(const char *program, char **args, int32_t *retval);
int sys_reboot(int code);
int sys___time(userptr_t user_seconds, userptr_t user_nanoseconds);
void copytf(void *tf, unsigned long);
void getpid(int32_t *retval);
int dup2(int oldfd, int newfd, int32_t *retval);
int rounded(int a);

#endif /* _SYSCALL_H_ */
