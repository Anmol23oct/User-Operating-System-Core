// File:	worker_t.h

// List all group member's name: Abhinay Reddy Vongur(av730), Anmol Sharma(as3593)
// username of iLab: ilabu3
// iLab Server: ilabu3.cs.rutgers.edu

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

#ifndef MLFQ
	#define SCHEDULER rr
#else
	#define SCHEDULER mlfq
#endif
/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1

#define _XOPEN_SOURCE 700

#define STACK_SIZE SIGSTKSZ

#define TIME_SLICE 20

#define MLFQ_LEVELS 4

#define PRIORITY_BOOST_CYCLES 30
/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>

typedef uint worker_t;

typedef struct QueueNode{
	struct TCB* node_tcb;
	struct QueueNode* next_node;
} node;

typedef struct Queue{
	struct QueueNode* head;
	struct QueueNode* tail;
} queue;

typedef enum{
	RUNNING,
	READY,
	BLOCKED,
	FINISHED
} thread_status;

typedef struct TCB {
	worker_t id; // holds the id of the thread
	thread_status status; // status of the thread
	ucontext_t* context; // context for the thread
	char* stack; // pointer to the stack created 
	uint priority; // priority of the thread
	worker_t waiting_thread; // holds any thread id waiting for its termination
	worker_t waiting_for; // holds the owner thread id of the mutex it is waiting for
	int first_schedule; // flag to indicate if scheduling the thread for first time
	struct timeval created, first_run, finished; // saves timestamps of different events of the thread
	double time_on_cpu; // time spent running on cpu
	int mutex_blocked; // flag to indicate if blocked by a mutex
	struct worker_mutex_t* blocked_from_mutex; // refernece to the mutex it is waiting for
	
} tcb; 

/* mutex struct definition */
typedef struct worker_mutex_t {

	// YOUR CODE HERE
	int is_locked; // flag to indicate if the mutex is locked
	uint id; // id of the mutex
	struct Queue* wait_queue; // queue to hold references to threads waiting for it
	worker_t owner; // id of the thread that currently has the mutex
} worker_mutex_t;

typedef enum{
	rr,
	mlfq,
} scheduler;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE


/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

#endif
