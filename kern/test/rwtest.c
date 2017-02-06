/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>

/*
 * Use these stubs to test your reader-writer locks.
 */

 static struct rwlock *rwlock;
static void lockread(void *junk, unsigned long num){
	(void)junk;
	(void)num;
	rwlock_acquire_read(rwlock);
	for(int i = 0; i < 25000; i++){

	}
	rwlock_release_read(rwlock);

}

static void lockwrite(void *junk, unsigned long num){
	(void)junk;
	(void)num;
	rwlock_acquire_write(rwlock);
	for(int i = 0; i < 2500; ++i){
		
	}
	rwlock_release_write(rwlock);
}
//Tests 1 and 2 work, but only if they are the very first test run in a given kernel session.
//None of the other tests work, and I don't think my math is wrong.
//Tests preform worse and worse until eventually panicking if run repeatedly in the same session.
int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;
	unsigned long i = 0;
	rwlock = rwlock_create("welcome");

	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);
	
	for(int i = 0; i < 1500000; ++i){
		
	}
	rwlock_destroy(rwlock);
	success(getTestVar() == 20 ? TEST161_SUCCESS:TEST161_FAIL, SECRET, "rwt1");

	return 0;
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;
	unsigned long i = 0;
	rwlock = rwlock_create("salutations");

	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);
	
	for(int i = 0; i < 1500000; ++i){
		
	}
	rwlock_destroy(rwlock);
	success(getTestVar() == 60 ? TEST161_SUCCESS:TEST161_FAIL, SECRET, "rwt2");

	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;
	unsigned long i = 0;
	rwlock = rwlock_create("hello");

	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);

	for(int i = 0; i < 1500000; ++i){

	}
	rwlock_destroy(rwlock);
	success(getTestVar() == 30 ? TEST161_SUCCESS:TEST161_FAIL, SECRET, "rwt3");

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;
	unsigned long i = 0;
	rwlock = rwlock_create("greetings");

	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);

	for(int i = 0; i < 1500000; ++i){

	}
	rwlock_destroy(rwlock);
	success(getTestVar() == 40 ? TEST161_SUCCESS:TEST161_FAIL, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;
	unsigned long i = 0;
	rwlock = rwlock_create("aloha");
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockwrite, 0, i);
	for(int i = 0; i < 1500000; ++i){

	}
	rwlock_destroy(rwlock);
	success(getTestVar() == 40 ? TEST161_SUCCESS:TEST161_FAIL, SECRET, "rwt5");

	return 0;
}



