/*
 * Copyright (c) 2001, 2002, 2009
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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * Called by the driver during initialization.
 */
	bool q1;
	bool q2;
	bool q3;
	bool q4;
	int wait; 
	int carsInAQuad;
	struct lock *lock;
	struct cv *cv;

void
stoplight_init() {
	carsInAQuad = 0;
	q1 = false;
	q2 = false;
	q3 = false;
	q4 = false;
	wait = 0;
	lock = lock_create("shit");
	cv = cv_create("poop");
	return;
}
// almost done need to stop the first asshole and let the last asshole shit. Corks not allowed.

/*
 * 
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	cv_destroy(cv);
	lock_destroy(lock);
	return;
}

void
turnright(uint32_t direction, uint32_t index)
{
	lock_acquire(lock);
	int quad = direction;
	bool single = true;
	while(getQ(quad)|| carsInAQuad >= 3 || single){
		// kprintf("sleepy");
		wait++;
		single = false;
		cv_broadcast(cv, lock);
		cv_wait(cv,lock);
		wait--;
	}
	carsInAQuad++;
	setQ(quad,true);
	inQuadrant(quad,index);
	if(carsInAQuad != 1){
				cv_broadcast(cv, lock);
		cv_wait(cv,lock);
	}
	setQ(quad,false);
	carsInAQuad--;
	leaveIntersection(index);
	cv_broadcast(cv, lock);
	lock_release(lock);
	(void)direction;
	(void)index;

	/*
	 * Implement this function.
	 */
	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{
	lock_acquire(lock);
	int quad = direction;
	bool single = true;
	while(getQ(quad)|| carsInAQuad >= 3  || single){
		wait++;
		single = false;
				// kprintf("sleepy");
		cv_broadcast(cv, lock);
		cv_wait(cv,lock);
		wait--;
	}
	carsInAQuad++;
	setQ(quad,true);
	inQuadrant(quad,index);
	cv_broadcast(cv, lock);	

	bool singleton = true;
	while((getQ(getNextQuadrant(quad, 1)) ||( singleton  && wait > 0))){
		singleton = false;
		wait++;
				cv_broadcast(cv, lock);
		cv_wait(cv, lock);
		wait--;
	}

	setQ(quad,false);
	quad = getNextQuadrant(quad,1);
	while(getQ(quad)){
		wait++;
				cv_broadcast(cv, lock);
		cv_wait(cv,lock);
		wait--;
	}
	setQ(quad,true);
	inQuadrant(quad,index);
	setQ(quad,false);
	
	carsInAQuad--;
	leaveIntersection(index);
	cv_broadcast(cv, lock);
	lock_release(lock);
	(void)direction;
	(void)index;
	/*
	 * Implement this function.
	 */
	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
	lock_acquire(lock);
	int quad = direction;
			bool single = true;
	while(getQ(quad)|| carsInAQuad >= 3 || single){
		single = false;
		wait++;
		cv_broadcast(cv, lock);
		cv_wait(cv,lock);
		wait--;
	}
	carsInAQuad++;
	setQ(quad,true);
	inQuadrant(quad,index);
	cv_broadcast(cv, lock);

	bool singleton = false;
	while((getQ(getNextQuadrant(quad, 2)) || singleton)){
		wait++;
		singleton = false;
				cv_broadcast(cv, lock);
		cv_wait(cv, lock);
		wait--;
	}

	setQ(quad,false);
	quad = getNextQuadrant(quad,2);
	setQ(quad,true);
	inQuadrant(quad,index);
	cv_broadcast(cv, lock);
	
	singleton = false;
	while((getQ(getNextQuadrant(quad, 2)) || singleton)){
		wait++;
		singleton = false;
				cv_broadcast(cv, lock);
		cv_wait(cv, lock);
		wait--;
	}

	setQ(quad,false);
	quad = getNextQuadrant(quad,2);
	while(getQ(quad)){
		wait++;
				cv_broadcast(cv, lock);
		cv_wait(cv,lock);

		wait--;
	}
	
	setQ(quad,true);
	inQuadrant(quad,index);

	setQ(quad,false);
	carsInAQuad--;
	leaveIntersection(index);
	cv_broadcast(cv, lock);
	lock_release(lock);
	(void)direction;
	(void)index;
	/*
	 * Implement this function.
	 */
	return;
}


//direction of travel map 
// 1 == straight   (x+3 % 4)
// 2 == turn left  use straight formula
// 3 == turn right reflexive
int getNextQuadrant(int currentQuadrant, int driectionOfTravel){
	int x = currentQuadrant;

	switch(driectionOfTravel){
		case 1:
			return (x+3)%4;
		break;
		case 2:
			return (x+3)%4;
		break;
		case 3:
			return x;
		break;
	}
	return 0;
}

void setQ(int q, bool value){
	switch(q){
		case 0:
			q1 = value;
		break;
		case 1:
			q2 = value;
		break;
		case 2:
			q3 = value;
		break;
		case 3:
			q4 = value;
		break;
	}
}

bool getQ(int q){
	switch(q){
		case 0:
			return q1;
		break;
		case 1:
			return q2;
		break;
		case 2:
			return q3;
		break;
		case 3:
			return q4;
		break;
	}

	return false;
}
