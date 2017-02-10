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

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.
static int testvar = 20;
struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	// kprintf("i exists");

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}
	lock->lock_wchan = wchan_create(lock->lk_name);
	if (lock->lock_wchan == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}
		lock->taken = false;
	spinlock_init(&lock->slock);

	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);

	// add stuff here as needed

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	//Make sure lock is valid and isn't being held
	KASSERT(lock != NULL);
	KASSERT(lock->taken == false);

	//Clean up all my variables
	kfree(lock->lk_name);
	spinlock_cleanup(&lock->slock);
	wchan_destroy(lock->lock_wchan);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{



	//Make sure the current thread exists
	KASSERT(curthread != NULL);
	//Make sure that the lock exists
	KASSERT(lock != NULL);
	//Lock down the variables
	spinlock_acquire(&lock->slock);
	//While someone has the lock, sleep all incomming threads
	while (lock->taken == true) {
		wchan_sleep(lock->lock_wchan, &lock->slock);
	}
	//Set ownership of the thread
	lock->currentThread = curthread;
	lock->taken = true;
	//Allow the variables to be modified again
	spinlock_release(&lock->slock);

	/* Call this (atomically) before waiting for a lock */
	//HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);

	// Write this

	(void)lock;  // suppress warning until code gets written

	/* Call this (atomically) once the lock is acquired */
	//HANGMAN_ACQUIRE(&curthread->t_hangman, &lock->lk_hangman);


}

void
lock_release(struct lock *lock)
{

	//Make sure the current thread exists
	KASSERT(curthread != NULL);
	//Make sure the lock exists
	KASSERT(lock != NULL);
	//Make sure the calling thread owns the lock
	KASSERT(lock_do_i_hold(lock));

	//Lock down the lock so no other thread can mess with the variables
	spinlock_acquire(&lock->slock);
	//Set taken back to false
	lock->taken = false;
	//Reset ownership of the thread
	lock->currentThread = NULL;
	//Wake a waiting thread so they can begin work
	wchan_wakeone(lock->lock_wchan, &lock->slock);
	//Let the variables be modified agian
	spinlock_release(&lock->slock);

	/* Call this (atomically) when the lock is released */
	//HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);

	// Write this


}

bool
lock_do_i_hold(struct lock *lock)
{
	

	//Make sure current thread exists
	KASSERT(curthread != NULL);
	//Make sure the lock exists
	KASSERT(lock != NULL);
	//Make sure the owner of the lock is calling for its release
	if(curthread == lock->currentThread){
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;


	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}
	cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}
	spinlock_init(&cv->cvlock);
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);
	// add stuff here as needed
	spinlock_cleanup(&cv->cvlock);
	wchan_destroy(cv->cv_wchan);
	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
    spinlock_acquire(&cv->cvlock);
    lock_release(lock);
    wchan_sleep(cv->cv_wchan, &cv->cvlock);
    spinlock_release(&cv->cvlock);
    lock_acquire(lock);

}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	KASSERT(lock_do_i_hold(lock));
	spinlock_acquire(&cv->cvlock);
	wchan_wakeone(cv->cv_wchan, &cv->cvlock);
	spinlock_release(&cv->cvlock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	KASSERT(lock_do_i_hold(lock));
	spinlock_acquire(&cv->cvlock);
	wchan_wakeall(cv->cv_wchan, &cv->cvlock);
	spinlock_release(&cv->cvlock);
}


struct rwlock * rwlock_create(const char *name){
	struct rwlock *rwlock;

	rwlock = kmalloc(sizeof(*rwlock));
	if (rwlock == NULL) {
		return NULL;
	}

	rwlock->rwlock_name = kstrdup(name);
	if (rwlock->rwlock_name == NULL) {
		kfree(rwlock);
		return NULL;
	}
	rwlock->readers = 0;
	rwlock->listIndex = 0;
	rwlock->writer = NULL; 
	rwlock->writeRequested = 0;
	rwlock->rwait = 0;
	rwlock->toggle = false;
	rwlock->lock = lock_create("shitter");
	rwlock->cv_read = cv_create("poo");
	rwlock->cv_write = cv_create("iwannadie");
	return rwlock;

}
void rwlock_destroy(struct rwlock *rwlock){
	//make sure that the lock is valid and isnt being held
	KASSERT(rwlock != NULL);

	// add stuff here as needed
	// kprintf("i died");

	// clean up all my varibles
	kfree(rwlock->rwlock_name);
	// kfree(rwlock->listIndex);
	// kfree(rwlock->writer);
	// kfree(rwlock->writeRequested);
	// kfree(rwlock->lock);
	kfree(rwlock);
}

void rwlock_acquire_read(struct rwlock *rwlock){
	lock_acquire(rwlock->lock);
	while(rwlock->writeRequested > 0|| rwlock->writer != NULL){
		rwlock->rwait++;
		cv_wait(rwlock->cv_read, rwlock->lock);
		rwlock->rwait--;
	}

	rwlock->readers++;
	rwlock->threadList[rwlock->listIndex] = curthread;
	rwlock->listIndex++;
	if(rwlock->listIndex +2 >= sizeof(rwlock->threadList)/sizeof(rwlock->threadList[0])){
		increaseArraySize(rwlock, rwlock->listIndex + 25);
	}
	lock_release(rwlock->lock);

	return;
}
void rwlock_release_read(struct rwlock *rwlock){
	KASSERT(amIReading(rwlock));
	lock_acquire(rwlock->lock);
	shiftArray(rwlock);
	rwlock->readers--;
	cv_signal(rwlock->cv_write, rwlock->lock);
	lock_release(rwlock->lock);
	return;
}
void rwlock_acquire_write(struct rwlock *rwlock){
	lock_acquire(rwlock->lock);
	while(rwlock->readers > 0 || rwlock->writer != NULL){
		cv_wait(rwlock->cv_write, rwlock->lock);
	}

	rwlock->writeRequested++;
	rwlock->writer = curthread;
	lock_release(rwlock->lock);

	return;
}
void rwlock_release_write(struct rwlock *rwlock){
	KASSERT(rwlock->writer == curthread);

	lock_acquire(rwlock->lock);
	rwlock->writeRequested--;
	rwlock->writer = curthread;
	rwlock->writer = NULL;

	if(rwlock->toggle || rwlock->rwait == 0){
		rwlock->toggle = false;
		cv_signal(rwlock->cv_write, rwlock->lock);
	}
	else{
		rwlock->toggle = true;
		cv_broadcast(rwlock->cv_read, rwlock->lock);
	}

	lock_release(rwlock->lock);
	return;
}

void increaseArraySize(struct rwlock* rwlock, int newSize){
			kprintf("increasing array size \n");
	struct thread* arr[newSize];
	for(unsigned int i = 0; i < sizeof(rwlock->threadList)/sizeof(rwlock->threadList[0]); ++i){
		arr[i] = rwlock->threadList[i];
		rwlock->threadList[i] = NULL;
	}
	// kfree(rwlock->threadList);
	*rwlock->threadList = *arr;

}

void shiftArray(struct rwlock* rwlock){
	bool threadFound = false;
	for(unsigned int i = 0; i < rwlock->listIndex; ++i){
		if(curthread == rwlock->threadList[i] || threadFound){
			threadFound = true;
			rwlock->threadList[i] = rwlock->threadList[i+1];
		}
	}
	rwlock->threadList[rwlock->listIndex - 1] = NULL;
	rwlock->listIndex--;
}

bool amIReading(struct rwlock* rwlock){
	for(unsigned int i = 0; i < rwlock->listIndex; ++i){
		if(curthread == rwlock->threadList[i]){
			return true;
		}
	}
	return false;
}

int getTestVar(){
	return testvar;
}
