// A simple pthreads test program to test basic API usage.
//
// This does not aim to be a complete test-suite, just the basics.

#include "../pthread.h"
#include "../semaphore.h"

#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define WORKERS 10

#define error(...) do { fprintf(stderr, __VA_ARGS__); exit(1); } while(0)
#define check(X) do { if (!(X)) error("Check failed at line %i.\n", __LINE__); } while(0)

void *inc_x(void *x_void_ptr)
{
	// increment x to 100
	int *x_ptr = (int *)x_void_ptr;
	while (++(*x_ptr) < 100);
	return NULL;
}

void test_create()
{
	int x = 0, y = 0;
	pthread_t inc_x_thread;

	if (pthread_create(&inc_x_thread, NULL, inc_x, &x))
		error("Error creating thread\n");

	while(++y < 100);

	if (pthread_join(inc_x_thread, NULL))
		error("Error joining thread\n");

	check(x == 100 && y == 100);
}

void test_cancel_cleanup(void *arg)
{
	int *i = (int *)arg;
	*i = 2;
}

void *test_cancel_thread(void *arg)
{
	int *i = (int *)arg;
	pthread_cleanup_push(test_cancel_cleanup, arg);
	SleepEx(INFINITE, TRUE);
	*i = 1;
	return NULL;
}

void test_cancel()
{
	pthread_t th;
	int arg = 0;

	if (pthread_create(&th, NULL, test_cancel_thread, &arg))
		error("Error creating thread\n");

	Sleep(100);

	pthread_cancel(th);
	pthread_join(th, NULL);
	check(arg == 2);
}

void *test_cancel_thread2(void *arg)
{
	pthread_cleanup_push(test_cancel_cleanup, arg);
	while (true)
	{
		Sleep(1);
		pthread_testcancel();
	}
}

void test_cancel2()
{
	pthread_t th;
	int arg = 0;

	if (pthread_create(&th, NULL, test_cancel_thread2, &arg))
		error("Error creating thread\n");

	Sleep(100);

	pthread_cancel(th);
	pthread_join(th, NULL);
	check(arg == 2);
}

pthread_spinlock_t spin;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int spincheck;

void *test_spin_thread(void *arg)
{
	int id = (int)arg;
	for (int n=0;n<100;n++)
	{
		Sleep((rand()>>8)%8);
		check(pthread_spin_lock(&spin) == 0);
		check(spincheck == 0);
		spincheck = id;
		Sleep((rand()>>8)%8);
		check(spincheck == id);
		spincheck = 0;
		check(pthread_spin_unlock(&spin) == 0);
	}
	return NULL;
}

void test_spin()
{
	pthread_t th[WORKERS];

	spincheck = 0;
	if (pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE))
		error("Error creating spinlock\n");

	for (int n=0;n<WORKERS;n++)
	{
		if (pthread_create(&th[n], NULL, test_spin_thread, (void *)n))
			error("Error creating thread\n");
	}
	for (int n=0;n<WORKERS;n++)
	{
		if (pthread_join(th[n], NULL))
			error("Error joining thread\n");
	}

	check(spincheck == 0);

	if (pthread_spin_destroy(&spin))
		error("Error destroying spinlock\n");
}

void *test_mutex_thread(void *arg)
{
	int id = (int)arg;
	for (int n=0;n<100;n++)
	{
		Sleep((rand()>>8)%8);
		check(pthread_mutex_lock(&mutex) == 0);
		check(spincheck == 0);
		spincheck = id;
		Sleep((rand()>>8)%8);
		check(spincheck == id);
		spincheck = 0;
		check(pthread_mutex_unlock(&mutex) == 0);
	}
	return NULL;
}

void test_mutex()
{
	pthread_t th[WORKERS];

	spincheck = 0;
	for (int n=0;n<WORKERS;n++)
	{
		if (pthread_create(&th[n], NULL, test_mutex_thread, (void *)n))
			error("Error creating thread\n");
	}
	for (int n=0;n<WORKERS;n++)
	{
		if (pthread_join(th[n], NULL))
			error("Error joining thread\n");
	}

	check(spincheck == 0);
}

pthread_key_t key;
bool key_deleted = false;

void key_destroy(void *value)
{
	key_deleted = true;
}

void *test_tls_thread(void *arg)
{
	check(pthread_setspecific(key, (void *)0x5678) == 0);
	Sleep(100);
	check(pthread_getspecific(key) == (void *)0x5678);
	return NULL;
}

void test_tls()
{
	check(pthread_key_create(&key, key_destroy) == 0);

	check(pthread_setspecific(key, (void *)0x1234) == 0);

	pthread_t th;
	check(pthread_create(&th, NULL, test_tls_thread, NULL) == 0);
	check(pthread_join(th, NULL) == 0);

	check(pthread_getspecific(key) == (void *)0x1234);

	check(key_deleted == true);
	check(pthread_key_delete(key) == 0);
}

pthread_barrier_t bar1, bar2;
LONG barwaiting1 = 0;
LONG barwaiting2 = 0;
LONG barfinished = 0;

void *test_barrier_thread(void *arg)
{
	InterlockedIncrement(&barwaiting1);
	pthread_barrier_wait(&bar1);
	InterlockedIncrement(&barwaiting2);
	pthread_barrier_wait(&bar2);
	InterlockedIncrement(&barfinished);
	return NULL;
}

void test_barrier()
{
	pthread_t th[WORKERS];

	check(pthread_barrier_init(&bar1, NULL, WORKERS+1) == 0);
	check(pthread_barrier_init(&bar2, NULL, WORKERS+1) == 0);

	for (int n=0;n<WORKERS;n++)
		check(pthread_create(&th[n], NULL, test_barrier_thread, NULL) == 0);

	Sleep(1000);

	// The workers should all now be waiting on bar1.
	check(barwaiting1 == WORKERS);
	check(barwaiting2 == 0);

	// Release bar1.
	pthread_barrier_wait(&bar1);

	Sleep(1000);

	// The workers should all now be waiting on bar2.
	check(barwaiting2 == WORKERS);
	check(barfinished == 0);

	// Release bar2.
	pthread_barrier_wait(&bar2);

	Sleep(1000);

	// The workers should all now be finished.
	check(barfinished == WORKERS);

	for (int n=0;n<WORKERS;n++)
		check(pthread_join(th[n], NULL) == 0);
}

pthread_rwlock_t rwlock;

void *test_rwlock_thread(void *arg)
{
	int id = (int)arg;
	for (int n=0;n<100;n++)
	{
		Sleep((rand()>>8)%8);
		bool write = (rand() & 0x1000) ? true : false;

		if (write)
		{
			check(pthread_rwlock_wrlock(&rwlock) == 0);
			check(spincheck == 0);
			spincheck = id;
			Sleep((rand()>>8)%8);

			// check no-one else modified it while we were sleeping
			check(spincheck == id);
			spincheck = 0;
			check(pthread_rwlock_unlock(&rwlock) == 0);
		} else {
			check(pthread_rwlock_rdlock(&rwlock) == 0);
			int prev = spincheck;
			Sleep((rand()>>8)%8);
			// check no-one at all modified it while we were sleeping
			check(prev == spincheck);
			check(pthread_rwlock_unlock(&rwlock) == 0);
		}
	}
	return NULL;
}

void test_rwlock()
{
	pthread_t th[WORKERS];

	check(pthread_rwlock_init(&rwlock, NULL) == 0);

	spincheck = 0;
	for (int n=0;n<WORKERS;n++)
	{
		if (pthread_create(&th[n], NULL, test_rwlock_thread, (void *)n))
			error("Error creating thread\n");
	}
	for (int n=0;n<WORKERS;n++)
	{
		if (pthread_join(th[n], NULL))
			error("Error joining thread\n");
	}

	check(spincheck == 0);

	check(pthread_rwlock_destroy(&rwlock) == 0);
}

struct Work {
	int i;
	Work *next;
};

pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
LONG seen[100];
Work *work;

void *cond_writer(void *arg) 
{
	// Generate N queue items.
	for (int i=0;i<100;i++)
	{
		// Generate a new work item.
		pthread_mutex_lock(&cond_mutex);
		Work *w = new Work;
		w->i = i;
		w->next = work;
		work = w;

		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&cond_mutex);
	}
	return NULL;
}

void test_cond()
{
	pthread_t th[WORKERS];
	
	pthread_attr_t attr;
	check(pthread_attr_init(&attr) == 0);
	check(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) == 0);
	check(pthread_attr_destroy(&attr) == 0);

	// Create writers.
	for (int i=0;i<WORKERS;i++)
		check(pthread_create(&th[i], &attr, cond_writer, NULL) == 0);

	// Process N queue items.
	for (int i=0;i<100*WORKERS;i++)
	{
		// Grab the next work item.
		pthread_mutex_lock(&cond_mutex);
		while (work == NULL)
			pthread_cond_wait(&cond, &cond_mutex);
		Work *todo = work;
		work = work->next;
		pthread_mutex_unlock(&cond_mutex);

		InterlockedIncrement(&seen[todo->i]);
		delete todo;
	}

	for (int i=0;i<WORKERS;i++)
		check(pthread_join(th[i], NULL) == 0);

	// Validate
	for (int i=0;i<100;i++)
		check(seen[i] == WORKERS);

	check(pthread_mutex_destroy(&cond_mutex) == 0);
	check(pthread_cond_destroy(&cond) == 0);
}

sem_t sem;
LONG posted;

void *test_sem_thread(void *arg)
{
	for (int n=0;n<100;n++)
	{
		check(sem_wait(&sem) == 0);
		InterlockedIncrement(&posted);
	}
	return NULL;
}

void test_sem()
{
	check(sem_init(&sem, 0, 0) == 0);

	pthread_t th;
	check(pthread_create(&th, NULL, test_sem_thread, NULL) == 0);

	for (int n=0;n<50;n++)
		check(sem_post(&sem) == 0);
	Sleep(1000);
	check(posted == 50);
	for (int n=0;n<50;n++)
		check(sem_post(&sem) == 0);

	check(pthread_join(th, NULL) == 0);
	check(sem_destroy(&sem) == 0);

	check(posted == 100);
}

#define TEST(NAME) printf("Testing %s...\n", #NAME); test_##NAME();

int main()
{
	TEST(create);
	TEST(cancel);
	TEST(cancel2);
	TEST(mutex);
	TEST(spin);
	TEST(tls);
	TEST(barrier);
	TEST(rwlock);
	TEST(cond);
	TEST(sem);
	return 0;
}
