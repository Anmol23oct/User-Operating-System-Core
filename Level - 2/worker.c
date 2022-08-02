// File:	worker.c

// List all group member's name: Abhinay Reddy Vongur(av730), Anmol Sharma(as3593)
// username of iLab: ilabu3
// iLab Server: ilabu3.cs.rutgers.edu

#include "worker.h"
#include <unistd.h>
#include <fcntl.h>
// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE

static uint thread_count = 0; // count of total threads created
static uint active_threads = 0; // number of threads that are currently active
static queue* ready_queue; // queue to hold ready threads for round robin
static queue* waiting_queue; // waiting queue to hold threads that are blocked
static queue MLFQ_queue[MLFQ_LEVELS]; // Array of queues for MLFQ
static tcb* running_thread; // reference to the currently running thread
static node* running_node; 
static ucontext_t* sched_context; // scheduler context
static tcb* main_thread; // reference to main thread
static ucontext_t* finished_context; 
static void *return_values[10000]; // array to hold return values

static uint mutex_count = 0; // count of mutexs created

struct itimerval timer_val; 
struct sigaction act_handler;
static int interrupt_disabled = 0; // flag to indicate if signals are to be ignored
static struct timeval schedule_timestamp; // timestamp of last schedule
static double avg_turnaround, avg_response; // global variables for turnaround and response times
static int schedule_cycles = 0; // to track the schedule cycles elapsed

static void enqueue(queue* enq_queue, node* enq_node);
static void pop_from_queue(queue* pop_queue, node** pop_node);
static void create_node(tcb* thread_tcb, node** assign_node);
static void find_thread(worker_t thread_id, queue* lookup_queue, tcb** return_thread);
static void delete_from_queue(queue* del_queue, worker_t thread_id, node** return_node);
static void release_threads(tcb* exit_thread);
static void unblock_threads(queue* mutex);
static void empty_queue(queue* em_queue);
static void interrupt_handler();
static void worker_finished();
static void disable_signal();
static void enable_signal();
static int init_timer(int time_slice);
static int setup_handler();
static void find_ready_threads(worker_t thread_id, tcb** return_thread);
static void update_time_stats();
static void get_next_thread_mlfq(node** next_node);
static void init();
static void schedule();
static void sched_rr();
static void sched_mlfq();
static void boost_priority();

void thread_wrapper(void *(*start)(void *), void *arg){
	start((void *)arg);
	printf("Exiting thread id: %d\n", running_thread->id);
	worker_exit(NULL);
}

/* create a new thread */
int worker_create(worker_t * tid, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
		
	if(thread_count == 0){
		//creating a main thread for calling function
		init();
		if (SCHEDULER == rr) {
			printf("Scheduler is Round Robin\n");
		} else {
			printf("Scheduler is MLFQ\n");
		}
		main_thread = (tcb*)malloc(sizeof(tcb));
		if(main_thread == NULL){
			printf("Failed to allocate memory for tcb of main thread\n");
			return -1;
		}
		main_thread->id = thread_count++;
		
		main_thread->context = (ucontext_t*)malloc(sizeof(ucontext_t));
		if(main_thread->context == NULL){
			printf("Failed to allocate memory for context of main thread\n");
			return -1;
		}

		if(getcontext(main_thread->context) == -1){
			printf("Failed to get context\n");
			return -1;
		}
		
		main_thread->status = RUNNING;
		main_thread->stack = main_thread->context->uc_stack.ss_sp;
		main_thread->context->uc_link = finished_context;
		main_thread->waiting_thread = -1;
		main_thread->waiting_for = -1;
		main_thread->finished.tv_sec = 0;
		main_thread->finished.tv_usec = 0;
		main_thread->first_schedule = 0;
		main_thread->mutex_blocked = 0;
		main_thread->blocked_from_mutex = NULL;
		gettimeofday(&main_thread->created, NULL);
		gettimeofday(&main_thread->first_run, NULL);

		active_threads++;
		avg_turnaround = 0;
		avg_response = 0;
		gettimeofday(&schedule_timestamp, NULL);
		
		main_thread->priority = 0;
		running_thread = main_thread;
		node* main_node;
		create_node(main_thread, &main_node);
		running_node = main_node;

		//setting up interrupt handler and the starting the timer
		setup_handler();
		init_timer(TIME_SLICE);
	}

	tcb *thread = (tcb*)malloc(sizeof(tcb));
	if(thread == NULL){
		printf("Failed to allocate memory for tcb of thread\n");
		return -1;
	}

	//Intialise TCB
	thread->context = (ucontext_t*)malloc(sizeof(ucontext_t));
	if(thread->context == NULL){
		printf("Failure to allocate memory for context\n");
		return -1;
	}

	thread->status = READY;
	thread->id = thread_count++;
	
	thread->priority = 0;

	thread->stack =  malloc(STACK_SIZE);
	if(thread->stack == NULL){
		printf("Failed to allocate memory for stack\n");
		return -1;
	}

	if(getcontext(thread->context) == -1){
		printf("Failed to get the context\n");
		return -1;
	}

	thread->context->uc_stack.ss_sp = thread->stack;
	thread->context->uc_stack.ss_size = STACK_SIZE;
	thread->context->uc_stack.ss_flags = 0;
	thread->waiting_thread = -1;
	thread->waiting_for = -1;
	thread->first_schedule = 1;
	thread->finished.tv_sec = 0;
	thread->finished.tv_usec = 0;
	thread->first_run.tv_sec = 0;
	thread->first_run.tv_usec = 0;
	thread->mutex_blocked = 0;
	thread->blocked_from_mutex = NULL;
	gettimeofday(&thread->created, NULL);

	*tid = thread->id;
	active_threads++;

	node* thread_node;
	create_node(thread, &thread_node);
	if (SCHEDULER == rr) {
		enqueue(ready_queue, thread_node);
	} else {
		enqueue(&MLFQ_queue[thread->priority], thread_node);
	}

	makecontext(thread->context, (void*)thread_wrapper,2,function, arg);
	thread->context->uc_link = 0;

	return thread->id;
};


/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	disable_signal();
	swapcontext(running_thread->context, sched_context);
	enable_signal();
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
	disable_signal();
	if (running_thread->status != RUNNING) {
		printf("Something went wrong. Non running thread called exit\n");
		exit(-1);
	}
	running_thread->status = FINISHED;
	if (value_ptr != NULL){
		return_values[running_thread->id] = value_ptr;
	}
	printf("Exiting thread id: %i\n", running_thread->id);
	worker_yield();
	enable_signal();
};

void print_queue_stat(queue* queue){
	node* temp_head = queue->head;
	int i = 0;
	while (temp_head != NULL) {
		printf("Thread id: %d at position %d Waiting for %d \n", temp_head->node_tcb->id, i, temp_head->node_tcb->waiting_for);
		i++;
		temp_head = temp_head->next_node;
	}
}

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE
	disable_signal();
	tcb* wait_on_thread = NULL;
	find_ready_threads(thread, &wait_on_thread);
	if (wait_on_thread == NULL){
		if ((int)thread <= thread_count){
			if (value_ptr!=NULL){
				*value_ptr = return_values[thread];	
			}
			enable_signal();
			return 0;
		}
		enable_signal();
		return -1;
	}

	if ((int)wait_on_thread->waiting_thread > 0){
		printf("Thread %i has been joined already by another thread\n", thread);
		return -1;
	}
	wait_on_thread->waiting_thread = running_thread->id;
	running_thread->status = BLOCKED;
	running_thread->waiting_for = wait_on_thread->id;
	enqueue(waiting_queue, running_node);
	worker_yield();
	enable_signal();
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE
	if (mutex == NULL) {
		printf("Mutex not initialised\n");
		mutex = (worker_mutex_t*) malloc(sizeof(worker_mutex_t));
	}
	mutex->is_locked = 0;
	mutex->owner = -1;
	mutex->id = mutex_count++;
	mutex->wait_queue = (queue*) malloc(sizeof(queue));
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
		if (mutex == NULL) {
			printf("Mutex has not been initialised\n");
			return -1;
		}
		
		while (1)
		{
			if (__sync_lock_test_and_set(&mutex->is_locked, 1) == 1){

				if (mutex->owner == running_thread->id) {
					printf("Mutex already acquired by same thread\n");
					return 0;
				}
				
				enqueue(mutex->wait_queue, running_node);
				
				running_thread->status = BLOCKED;
				running_thread->waiting_for = mutex->owner;
				running_thread->blocked_from_mutex = mutex;
				running_thread->mutex_blocked = 1;
				
				enqueue(waiting_queue, running_node);
				
				worker_yield();
			} else {
				mutex->owner = running_thread->id;
				mutex->is_locked = 1;
				if (mutex->owner == -1 || mutex->is_locked == 0){
					printf("Owner not set properly\n");
					printf("Running id: %i\n", running_thread->id);
					exit(-1);
				}
				running_thread->blocked_from_mutex = NULL;
				running_thread->mutex_blocked = 0;
				return 0;
		}
		}
		
		
        
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	if (mutex == NULL) {
		printf("Mutex has not been initialised\n");
		return -1;
	}

	if (mutex->is_locked) {
		if (mutex->owner != running_thread->id) {
			printf("Mutex is locked by a different thread\n");
			return -1;
		}
		mutex->is_locked = 0;
		mutex->owner = -1;
		if (mutex->wait_queue->head != NULL){
			unblock_threads(mutex->wait_queue);
			empty_queue(mutex->wait_queue);
		}

	} else {
		printf("Mutex is not locked\n");
		return -1;
	}
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init
	if (mutex == NULL) {
		printf("Invalid mutex\n");
		return -1;
	}

	if (mutex->is_locked) {
		printf("Mutex can not be destroyed. Currently acquired by another thread\n");
		return -1;
	}

	free(mutex->wait_queue);
	mutex = NULL;
	return 0;
};

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (RR or MLFQ)

	disable_signal();

	gettimeofday(&running_thread->finished, NULL);
	double time_ms;
	time_ms = (double)(running_thread->finished.tv_sec - schedule_timestamp.tv_sec) * 1000;
	if (time_ms == 0){
		time_ms += (double)(running_thread->finished.tv_usec - schedule_timestamp.tv_usec) / 1000;
	} else {
		time_ms -= 1000;
		time_ms += (double)((1000000 - schedule_timestamp.tv_usec) + running_thread->finished.tv_usec) / 1000;
	}
	running_thread->time_on_cpu += time_ms;

	if (SCHEDULER == rr){
		enable_signal();
		sched_rr();
	}
	 else {
		 enable_signal();
		 sched_mlfq();
	 }

}

/* Round-robin (RR) scheduling algorithm */
static void sched_rr() {
	// - your own implementation of RR
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	disable_signal();
	node* next_node = NULL;
	tcb* next_thread = NULL;
	tcb* prev_thread = running_thread;
	node* prev_node = running_node;
	
	int finished = 0;

	if (running_thread->status == FINISHED) {
		finished = 1;
		release_threads(prev_thread);
		update_time_stats();
		free(prev_thread);
		free(prev_node);
	} 
	else {
		if (prev_thread->status != BLOCKED){
			prev_thread->status = READY;
			enqueue(ready_queue, prev_node);
		}
	}
	pop_from_queue(ready_queue, &next_node);
	if (next_node == NULL) {
		enable_signal();
		if (main_thread != NULL){
			running_thread = main_thread;
			create_node(running_thread, &running_node);
			running_thread->status = RUNNING;
			setcontext(running_thread->context);
		}
		printf("No more ready threads to be scheduled\n");
		exit(0);
	}
	// next_node->next_node = NULL;
	next_thread = next_node->node_tcb;
	if (next_thread->status == BLOCKED){
		printf("Waiting thread came up for scheduling. Something went wrong id: %d\n", next_thread->id);
		exit(-1);
	}
	next_thread->status = RUNNING;
	
	running_node = next_node;
	running_thread = next_thread;
	if (running_thread->first_schedule) {
		gettimeofday(&running_thread->first_run, NULL);
		running_thread->first_schedule = 0;
	}
	enable_signal();
	init_timer(TIME_SLICE);
	gettimeofday(&schedule_timestamp, NULL);
	if (setcontext(running_thread->context)==-1){
		printf("Unable to shcedule the thread\n");
		exit(-1);
	}
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	node* next_node = NULL;
	tcb* next_thread = NULL;
	tcb* prev_thread = running_thread;
	node* prev_node = running_node;
	int finished = 0;

	schedule_cycles += 1;

	if (prev_thread->status == FINISHED) {
		release_threads(prev_thread);
		update_time_stats();
		free(prev_thread->stack);
		free(prev_thread);
	} else {
		if (prev_thread->status == RUNNING) {
			int new_priority = prev_thread->priority;
			if (prev_thread->time_on_cpu >= TIME_SLICE * (new_priority+1) && prev_thread->priority > MLFQ_LEVELS-1){
				prev_thread->time_on_cpu = 0;
				new_priority++;
				prev_thread->priority = new_priority;
			}
			prev_thread->status = READY;
			enqueue(&MLFQ_queue[new_priority], prev_node);
		}
		
	}

	if (schedule_cycles >= PRIORITY_BOOST_CYCLES) {
		boost_priority();
		schedule_cycles = 0;
	}
	get_next_thread_mlfq(&next_node);
	
	if (next_node == NULL) {
		enable_signal();
		if (main_thread!=NULL){
			running_thread = main_thread;
			create_node(running_thread, &running_node);
			running_thread->status = RUNNING;
			setcontext(running_thread->context);
		}
		printf("No threads to be scheduled\n");
		exit(0);
	}
	next_thread = next_node->node_tcb;

	if (next_thread->status == BLOCKED) {
		printf("Waiting thread came up for scheduling. id: %i\n", next_thread->id);
		int a = 0;
		exit(-1);
	}
	if (next_thread->first_schedule) {
		gettimeofday(&next_thread->first_run, NULL);
		next_thread->first_schedule = 0;
	}
	
	next_thread->status = RUNNING;
	running_thread = next_thread;
	running_node = next_node;
	init_timer(TIME_SLICE*(running_thread->priority+1));
	enable_signal();
	gettimeofday(&schedule_timestamp, NULL);
	if (setcontext(running_thread->context)==-1){
		printf("Unable to shcedule the thread\n");
		exit(-1);
	}

}

// Feel free to add any other functions you need

// YOUR CODE HERE

static void get_next_thread_mlfq(node** next_node){
	for(int i=0;i<MLFQ_LEVELS;i++){
		if (MLFQ_queue[i].head != NULL) {
			pop_from_queue(&MLFQ_queue[i], next_node);
				return;
		}
	}

}


static void interrupt_handler(){
	if (interrupt_disabled) {
		printf("Interrupt disabled. Ignoring signal\n");
		return;
	} else {
		if (swapcontext(running_thread->context, sched_context) == -1){
			printf("Error while swapping context\n");
			exit(-1);
		}
	}
}
static void worker_finished(){
	printf("Main exiting\n");
	running_thread->status = FINISHED;
	schedule();
}

static void disable_signal() {
	interrupt_disabled = 1;
}

static void enable_signal() {
	interrupt_disabled = 0;
}

static int setup_handler(){
	act_handler.sa_flags=0;
	act_handler.sa_handler=interrupt_handler;
	sigaction(SIGPROF,&act_handler, NULL);
	return 1;
}

static int init_timer(int time_slice){
	timer_val.it_value.tv_sec = 0;
	timer_val.it_value.tv_usec = (time_slice*1000) % 100000;
	timer_val.it_interval.tv_sec = 0;
	timer_val.it_interval.tv_usec = (time_slice*1000) % 100000;
	
	if (setitimer(ITIMER_PROF, &timer_val,NULL) == -1){
		printf("Error in Setting Timer\n");
		exit(-1);
	} 
	return 0;
}

static void empty_queue(queue* em_queue){
	em_queue->head = NULL;
	em_queue->tail = NULL;
}

static void unblock_threads(queue* mutex_wait_queue) {
	node* temp_head = mutex_wait_queue->head;
	node* enq_node = NULL;
	int node_id = -1;
	tcb* test_thread;
	while (temp_head != NULL) {
		node_id = temp_head->node_tcb->id;
		temp_head = temp_head->next_node;
		enq_node = NULL;
		delete_from_queue(waiting_queue, node_id, &enq_node);
		if (enq_node != NULL) {
			if (enq_node->node_tcb->id != node_id){
				printf("Error\n");
				delete_from_queue(waiting_queue, node_id, &enq_node);
			}
			enq_node->node_tcb->status = READY;
			enq_node->node_tcb->waiting_for = -1;
			if (SCHEDULER == rr){
				enqueue(ready_queue, enq_node);
			} else {
				enqueue(&MLFQ_queue[enq_node->node_tcb->priority], enq_node);
			}
			test_thread = NULL;
			find_ready_threads(node_id, &test_thread);
			if (test_thread == NULL){
				printf("Error\n");
				test_thread = NULL;
			}
		}
	}
	if (node_id >= 0){
		node_id = -1;
	}
}

static void release_threads(tcb* exit_thread){
	if ((int)exit_thread->waiting_thread >= 0){
		printf("Changing thread %d from blocked to ready\n", exit_thread->waiting_thread);
		worker_t thread_id = exit_thread->waiting_thread;
		node* release_node = NULL;
		delete_from_queue(waiting_queue, thread_id, &release_node);
		if (release_node != NULL) {
			release_node->next_node = NULL;
			release_node->node_tcb->status = READY;
			release_node->node_tcb->waiting_for = -1;
			if (SCHEDULER == rr){
				enqueue(ready_queue, release_node);
			} else {
				enqueue(&MLFQ_queue[release_node->node_tcb->priority], release_node);
			}
		}
	}
}

static void enqueue(queue* enq_queue, node* enq_node){
	
	if (enq_queue->head == NULL){
		enq_queue->head = enq_node;
		enq_queue->tail = enq_node;
	} else {
		if (enq_queue->tail == NULL) {
			node* temp_head = enq_queue->head;
			while (temp_head->next_node != NULL)
			{
				temp_head = temp_head->next_node;
			}
			enq_queue->tail = temp_head;
		}
		enq_queue->tail->next_node = enq_node;
		enq_queue->tail = enq_node;
	}
	
}

static void pop_from_queue(queue* pop_queue, node** pop_node) {
	if (pop_queue->head == NULL){
		return;
	}
	*pop_node = pop_queue->head;
	node* temp;
	temp = pop_queue->head->next_node;
	pop_queue->head->next_node = NULL;
	if (pop_queue->head == pop_queue->tail){
		pop_queue->head = NULL;
		pop_queue->tail = NULL;
	} else {
	pop_queue->head = temp;
	}
}

static void delete_from_queue(queue* del_queue, worker_t thread_id, node** return_node){
	node* head = del_queue->head;
	node* prev = NULL;

	while (head != NULL && head->node_tcb->id != thread_id) {
		prev = head;
		head = head->next_node;
	}

	if (head != NULL){
		node* temp = head->next_node;
		head->next_node = NULL;
		*return_node = head;
		if (head == del_queue->head) {
			if (del_queue->head == del_queue->tail){
				del_queue->head = NULL;
				del_queue->tail = NULL;
			}
			else {
				del_queue->head = del_queue->head->next_node;
			}
		} else if(head == del_queue->tail) {
			prev->next_node = NULL;
			del_queue->tail = prev;
		} else {
			prev->next_node = temp;
		}
	}
	
}

static void create_node(tcb* thread_tcb, node** assign_node) {
	node* new_node = (node*) malloc(sizeof(node));
	new_node->node_tcb = thread_tcb;
	new_node->next_node = NULL;
	*assign_node = new_node;
}

static void find_ready_threads(worker_t thread_id, tcb** return_thread){
	if (SCHEDULER == rr) {
		find_thread(thread_id, ready_queue, return_thread);
	} else {
		int i = 0;
		while (i < MLFQ_LEVELS) {
			find_thread(thread_id, &MLFQ_queue[i], return_thread);
			if (*return_thread != NULL) {
				return;
			}
			i++;
		}
	}
}

static void find_thread(worker_t thread_id, queue* lookup_queue, tcb** return_thread){
	node* queue_head = lookup_queue->head;
	while (queue_head != NULL) {
		if (queue_head->node_tcb->id == thread_id){
			*return_thread = queue_head->node_tcb;
			break;
		}
		queue_head = queue_head->next_node;
	}
}

static void update_time_stats() {
	double turnaround = (double)((running_thread->finished.tv_sec - running_thread->created.tv_sec) * 1000);
	if (turnaround == 0){
		turnaround += (double)((running_thread->finished.tv_usec - running_thread->created.tv_usec) / 1000);
	} else {
		turnaround -= 1000;
		turnaround += (double)(((1000000 - running_thread->created.tv_usec) + running_thread->finished.tv_usec) / 1000);
	}
	

	double response = (double)((running_thread->first_run.tv_sec - running_thread->created.tv_sec) * 1000);
	if (response == 0){
		response += (double)((running_thread->first_run.tv_usec - running_thread->created.tv_usec) / 1000);
	} else {
		response -= 1000;
		response += (double)(((1000000 - running_thread->created.tv_usec) + running_thread->first_run.tv_usec) / 1000);
	}

	int threads_finished = thread_count - active_threads;
	avg_turnaround = ((threads_finished * avg_turnaround) + turnaround) / (threads_finished + 1);
	avg_response = ((threads_finished * avg_response) + response) / (threads_finished + 1);
	printf("Average turnaround time is %lfms\n", avg_turnaround);
	printf("Average response time is %lfms\n", avg_response);
	active_threads--;
}

static void init() {

		sched_context = (ucontext_t*) malloc(sizeof(ucontext_t));
		sched_context->uc_stack.ss_sp = (void*) malloc(STACK_SIZE*2);
		if (getcontext(sched_context) < 0){
			printf("error getting context\n");
			exit(-1);
		}
		sched_context->uc_stack.ss_size = STACK_SIZE*2;
		sched_context->uc_stack.ss_flags = 0;
		sched_context->uc_link = 0;
		makecontext(sched_context, (void *)&schedule, 0);

		finished_context = (ucontext_t*) malloc(sizeof(ucontext_t));
		finished_context->uc_stack.ss_sp = (void*) malloc(STACK_SIZE*2);
		if (getcontext(finished_context) < 0){
			printf("error getting context\n");
			exit(1);
		}
		
		finished_context->uc_link = 0;
		finished_context->uc_stack.ss_size = STACK_SIZE*2;
		finished_context->uc_stack.ss_flags = 0;
		makecontext(finished_context, (void *)&worker_finished, 0);

		ready_queue = (queue*) malloc(sizeof(queue));
		waiting_queue = (queue*) malloc(sizeof(queue));
}

static void boost_priority(){
	int i = 1;
	node* new_node = NULL;
	
	while (i < MLFQ_LEVELS){
		if (MLFQ_queue[i].head != NULL) {
			new_node = NULL;
			pop_from_queue(&MLFQ_queue[i], &new_node);
			while (new_node != NULL)
			{
				// new_node->next_node = NULL;
				enqueue(&MLFQ_queue[0], new_node);
				new_node->node_tcb->priority = 0;
				new_node = NULL;
				pop_from_queue(&MLFQ_queue[i], &new_node);
			}
		}
		i++;
	}

}




