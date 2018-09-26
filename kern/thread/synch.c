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
	KASSERT(name != NULL);

	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

	lock->lk_wchan = wchan_create(lock->lk_name);
	if (lock->lk_wchan == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	spinlock_init(&lock->lk_spinlock);

	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);

	lock->lk_holder = NULL;

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);

	// TODO NEED TO GET THE SPINLOCK?
	spinlock_acquire(&lock->lk_spinlock);
	// Lock should not be in use while destroying.
	KASSERT(lock->lk_holder == NULL);
	spinlock_release(&lock->lk_spinlock);

	spinlock_cleanup(&lock->lk_spinlock);
	wchan_destroy(lock->lk_wchan);

	kfree(lock->lk_name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	KASSERT(lock != NULL);
	KASSERT(curthread != NULL);
	KASSERT(curthread->t_in_interrupt == false);
	// Make sure lock wasn't already acquired by the same thread.
	KASSERT(lock_do_i_hold(lock) == false);

	spinlock_acquire(&lock->lk_spinlock);
	HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);

	// Lock holder being non-NULL is indicative of the lock being held.
	// Loop on the lock being held for robustness.
	while (lock->lk_holder != NULL) {
		wchan_sleep(lock->lk_wchan, &lock->lk_spinlock);
	}

	lock->lk_holder = curthread;

	HANGMAN_ACQUIRE(&curthread->t_hangman, &lock->lk_hangman);
	spinlock_release(&lock->lk_spinlock);
}

void
lock_release(struct lock *lock)
{
	KASSERT(lock != NULL);
	spinlock_acquire(&lock->lk_spinlock);

	KASSERT(curthread != NULL);
	KASSERT(lock->lk_holder == curthread);

	wchan_wakeone(lock->lk_wchan, &lock->lk_spinlock);

	// Indicates lock release
	lock->lk_holder = NULL;

	HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);
	spinlock_release(&lock->lk_spinlock);
}

bool
lock_do_i_hold(struct lock *lock)
{
	bool held = false;

	KASSERT(lock != NULL);

	spinlock_acquire(&lock->lk_spinlock);

	held = (lock->lk_holder == curthread);

	spinlock_release(&lock->lk_spinlock);

	return held;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	KASSERT(name != NULL);

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name == NULL) {
		kfree(cv);
		return NULL;
	}

	cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	spinlock_init(&cv->cv_lock);

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);

	wchan_destroy(cv->cv_wchan);
	spinlock_cleanup(&cv->cv_lock);

	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(curthread != NULL);
	KASSERT(!curthread->t_in_interrupt);

	spinlock_acquire(&cv->cv_lock);

	KASSERT(lock_do_i_hold(lock));

	// Release the lock so the thread can wait until released 
	// by some other thread that needs the lock.
	lock_release(lock);

	wchan_sleep(cv->cv_wchan, &cv->cv_lock);

	spinlock_release(&cv->cv_lock);

	lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);

	spinlock_acquire(&cv->cv_lock);

	KASSERT(lock_do_i_hold(lock));

	wchan_wakeone(cv->cv_wchan, &cv->cv_lock);

	spinlock_release(&cv->cv_lock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);

	spinlock_acquire(&cv->cv_lock);

	KASSERT(lock_do_i_hold(lock));

	wchan_wakeall(cv->cv_wchan, &cv->cv_lock);

	spinlock_release(&cv->cv_lock);
}

////////////////////////////////////////////////////////////
//
// RW Lock

struct rwlock * 
rwlock_create(const char *name) 
{
	struct rwlock *rwl;

	KASSERT(name != NULL);

	rwl = kmalloc(sizeof(*rwl));
	if (rwl == NULL) {
		return NULL;
	}

	rwl->rwl_name = kstrdup(name);
	if (rwl->rwl_name == NULL) {
		kfree(rwl);
		return NULL;
	}
	
	rwl->reader_wchan = wchan_create(rwl->rwl_name);
	if (rwl->reader_wchan == NULL) {
		kfree(rwl->rwl_name);
		kfree(rwl);
		return NULL;
	}

	rwl->writer_wchan = wchan_create(rwl->rwl_name);
	if (rwl->writer_wchan == NULL) {
		kfree(rwl->rwl_name);
		wchan_destroy(rwl->reader_wchan);
		kfree(rwl);
		return NULL;
	}

	spinlock_init(&rwl->rwl_lock);

	rwl->writers_active = 0;
	rwl->readers_active = 0;
	rwl->writers_waiting = 0;
	rwl->readers_waiting = 0;
	
	return rwl;
}

void 
rwlock_destroy(struct rwlock *rwl)
{
	KASSERT(rwl != NULL);
	// Make sure the lock is not in use.
	KASSERT(rwl->writers_waiting + rwl->writers_active == 0);
	KASSERT(rwl->readers_waiting + rwl->readers_active == 0);

	wchan_destroy(rwl->reader_wchan);
	wchan_destroy(rwl->writer_wchan);
	spinlock_cleanup(&rwl->rwl_lock);

	kfree(rwl->rwl_name);
	kfree(rwl);
}

/*
 * Operations:
 *    rwlock_acquire_read  - Get the lock for reading. Multiple threads can
 *                          hold the lock for reading at the same time.
 *    rwlock_release_read  - Free the lock. 
 *    rwlock_acquire_write - Get the lock for writing. Only one thread can
 *                           hold the write lock at one time.
 *    rwlock_release_write - Free the write lock.
 *
 * These operations must be atomic. You get to write them.
 */


void 
rwlock_acquire_read(struct rwlock *rwl) 
{
	KASSERT(rwl != NULL);
	KASSERT(curthread != NULL);
	KASSERT(!curthread->t_in_interrupt);

	spinlock_acquire(&rwl->rwl_lock);

	if (rwl->writers_waiting + rwl->writers_active > 0) {
		++rwl->readers_waiting;
		wchan_sleep(rwl->reader_wchan, &rwl->rwl_lock);
		--rwl->readers_waiting;
	}

	KASSERT(rwl->writers_active == 0);

	++rwl->readers_active;

	spinlock_release(&rwl->rwl_lock);
}

void 
rwlock_release_read(struct rwlock *rwl)
{
	KASSERT(rwl != NULL);
	KASSERT(curthread != NULL);

	spinlock_acquire(&rwl->rwl_lock);

	--rwl->readers_active;

	if (rwl->writers_waiting > 0 && rwl->readers_active == 0) {
		wchan_wakeone(rwl->writer_wchan, &rwl->rwl_lock);
	}
	else if (rwl->writers_waiting == 0) {
		wchan_wakeall(rwl->reader_wchan, &rwl->rwl_lock); // TODO WAKEALL OR WAKEONE, WHICH WOULD CAUSE LESS STARVATION OF WRITERS?
	}

	spinlock_release(&rwl->rwl_lock);
}

void 
rwlock_acquire_write(struct rwlock *rwl)
{
	KASSERT(rwl != NULL);
	KASSERT(curthread != NULL);
	KASSERT(!curthread->t_in_interrupt);

	spinlock_acquire(&rwl->rwl_lock);

	if (rwl->writers_waiting + rwl->writers_active > 0 || rwl->readers_waiting + rwl->readers_active > 0) {
		++rwl->writers_waiting;
		wchan_sleep(rwl->writer_wchan, &rwl->rwl_lock);
		--rwl->writers_waiting;
	}

	KASSERT(rwl->readers_active == 0);
	KASSERT(rwl->writers_active == 0);

	rwl->writers_active = 1;

	spinlock_release(&rwl->rwl_lock);
}

void 
rwlock_release_write(struct rwlock *rwl)
{
	KASSERT(rwl != NULL);
	KASSERT(curthread != NULL);

	spinlock_acquire(&rwl->rwl_lock);

	rwl->writers_active = 0;

	if (rwl->readers_waiting > 0) {
		wchan_wakeone(rwl->reader_wchan, &rwl->rwl_lock);
	}

	else {
		wchan_wakeone(rwl->writer_wchan, &rwl->rwl_lock);
	}

	spinlock_release(&rwl->rwl_lock);
}
