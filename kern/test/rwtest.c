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

#define NREADER_LOOPS 60
#define NWRITER_LOOPS 60
#define NTHREADS 5

static volatile unsigned long testval1 = 0;

static struct rwlock * rw = NULL;
static struct semaphore * donesem = NULL;

static
void
rwlock_read_thread(void *junk, unsigned long num)
{
	int i;

	(void)junk;

	random_yielder(4);

	rwlock_acquire_read(rw);

	kprintf("ReadThread %lu: ", num);

	for (i = 0; i < NREADER_LOOPS; ++i) {
		kprintf("%lu ", testval1);
	}

	kprintf("\n");

	rwlock_release_read(rw);

	V(donesem);
}

static
void
rwlock_write_thread(void *junk, unsigned long num) 
{
	int i;

	(void)junk;

	random_yielder(4);

	rwlock_acquire_write(rw);

	kprintf("WriteThread %lu: ", num);

	for (i = 0; i < NWRITER_LOOPS; ++i) {
		++testval1;
		kprintf("[%lu] ", testval1);
	}

	kprintf("\n");

	rwlock_release_write(rw);

	V(donesem);
}

static 
void 
init_items() 
{
	if (donesem == NULL) {
		donesem = sem_create("donesem", 0);
		if (donesem == NULL) {
			panic("rwtest: sem_create failed\n");
		}
	}

	if (rw == NULL) {
		rw = rwlock_create("rwlocktest lock");
		if (rw == NULL) {
			panic("rwtest: rwlock_create failed\n");
		}
	}
}

int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

	int i;
	int result;

	init_items();

	testval1 = 0;

	kprintf("Starting rwt1...\n");

	for (i = 0; i < NTHREADS; ++i) {
		result = thread_fork("rwlock_reader", NULL, rwlock_read_thread, NULL, i);

		if (result) {
			panic("rwlock_reader: thread_fork failed: %s\n", strerror(result));
		}
	}

	for (i = 0; i < NTHREADS; ++i) {
		result = thread_fork("rwlock_writer", NULL, rwlock_write_thread, NULL, i);

		if (result) {
			panic("rwlock_writer: thread_fork failed: %s\n", strerror(result));
		}
	}

	for (i = 0; i < NTHREADS + NTHREADS; ++i) {
		P(donesem);
	}

	kprintf("rwt1 test done\n");

	return 0;
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt2 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt2");

	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt3 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt3");

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt4 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt5 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt5");

	return 0;
}
