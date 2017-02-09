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
static int test = 0;
static int writeshappend = 0;
static void lockread(void *junk, unsigned long num){
	(void)junk;
	(void)num;
	rwlock_acquire_read(rwlock);
	test = test + 0;
	KASSERT(test == 5 * writeshappend);
	rwlock_release_read(rwlock);
}
static void longRead(void *junk,unsigned long num){
	(void)junk;
	(void)num;
	rwlock_acquire_read(rwlock);
	test = test + 0;
	KASSERT(test == 5 * writeshappend);

	for(int i = 0; i < 15000000; ++i){

	}
	rwlock_release_read(rwlock);
}

static void lockwrite(void *junk, unsigned long num){
	(void)junk;
	(void)num;
	rwlock_acquire_write(rwlock);
	test = test + 5;
	writeshappend++;
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

	thread_fork("synchtest", NULL, longRead, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockwrite, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);

	for(int i=0; i < 140000000; ++i){

	}

	rwlock_destroy(rwlock);
	kprintf("%d\n", test);
	success(test == 10 ? TEST161_SUCCESS:TEST161_FAIL, SECRET, "rwt1");
	test = 0;
	writeshappend = 0;
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

	for(int i = 0; i < 1500000; ++i){

	}
	
	rwlock_destroy(rwlock);
	kprintf("%d\n", test);
	success(test == 20 ? TEST161_SUCCESS:TEST161_FAIL, SECRET, "rwt2");
	test = 0;
	writeshappend = 0;
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
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockwrite, 0, i);
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	thread_fork("synchtest", NULL, lockread, 0, i);	
	// thread_fork("synchtest", NULL, lockwrite, 0, i);

	for(int i = 0; i < 1500000; ++i){

	}

	rwlock_destroy(rwlock);

	kprintf("%d\n", test);
	success(test == 10 ? TEST161_SUCCESS:TEST161_FAIL, SECRET, "rwt3");
	test = 0;
	writeshappend = 0;
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
	kprintf("%d\n", test);
	success(test == 20 ? TEST161_SUCCESS:TEST161_FAIL, SECRET, "rwt4");
	test = 0;
	writeshappend = 0;
	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;
	unsigned long i = 0;
	rwlock = rwlock_create("aloha");
			thread_fork("synchtest", NULL, lockread, 0, i);			thread_fork("synchtest", NULL, lockread, 0, i);			thread_fork("synchtest", NULL, lockread, 0, i);			thread_fork("synchtest", NULL, lockwrite, 0, i);	
	while(test == 0){
		thread_fork("synchtest", NULL, lockread, 0, i);	
	}
	for(int i = 0; i < 15000000; ++i){

	}
	rwlock_destroy(rwlock);
	kprintf("%d\n", test);
	success(test == 5 * writeshappend ? TEST161_SUCCESS:TEST161_FAIL, SECRET, "rwt5");
	test = 0;
	writeshappend = 0;
	return 0;
}



