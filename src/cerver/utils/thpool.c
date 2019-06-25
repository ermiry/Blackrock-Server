/* ********************************
 * Original Author:       Johan Hanssen Seferidis
 * License:	     MIT
 *
 ********************************/

#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include "cerver/utils/thpool.h"
#include "cerver/utils/log.h"

#ifdef THPOOL_DEBUG
#define THPOOL_DEBUG 1
#else
#define THPOOL_DEBUG 0
#endif

#if !defined(DISABLE_PRINT) || defined(THPOOL_DEBUG)
#define err(str) fprintf(stderr, str)
#else
#define err(str)
#endif

static volatile int threads_keep_alive;
static volatile int threads_on_hold;

static threadpool *thpool_new (unsigned int num_threads) {

	threadpool *thpool = (threadpool *) malloc (sizeof (threadpool));
	if (thpool) {
		memset (thpool, 0, sizeof (threadpool));
		thpool->threads = (thread **) calloc (num_threads, sizeof (thread));
		thpool->job_queue = NULL;
	}

	return thpool;

}

// TODO:
static void thpool_delete (threadpool *thpool) {

	if (thpool) {
		free (thpool);
	}

}

// creates a new threadpool
threadpool *thpool_create (unsigned int num_threads) {

	threadpool *thpool = thpool_new (num_threads);
	if (thpool) {
		thpool->num_threads_alive = 0;
		thpool->num_threads_working = 0;

		thpool->job_queue = jobqueue_init ();
		if (!thpool->job_queue) {
			#ifdef CERVER_DEBUG
			cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
				"Failed to init thpool's job queue!");
			#endif
			thpool_delete (thpool);
		}

		else {
			pthread_mutex_init (&(thpool->thcount_lock), NULL);
			pthread_cond_init (&thpool->threads_all_idle, NULL);

			// init threads
			for (unsigned int i = 0; i < num_threads; i++)
				thread_init (thpool, &thpool->threads[i], i);

			// wait for threads to init
			while (thpool->num_threads_alive != num_threads) {}
		}

		threads_on_hold = 0;
		threads_keep_alive = 1;
	}

	return thpool;

}

/* Add work to the thread pool */
int thpool_add_work(thpool_* thpool_p, void (*function_p)(void*), void* arg_p){
	job* newjob;

	newjob=(struct job*)malloc(sizeof(struct job));
	if (newjob==NULL){
		err("thpool_add_work(): Could not allocate memory for new job\n");
		return -1;
	}

	/* add function and argument */
	newjob->function=function_p;
	newjob->arg=arg_p;

	/* add job to queue */
	jobqueue_push(&thpool_p->jobqueue, newjob);

	return 0;
}


/* Wait until all jobs have finished */
void thpool_wait(thpool_* thpool_p){
	pthread_mutex_lock(&thpool_p->thcount_lock);
	while (thpool_p->jobqueue.len || thpool_p->num_threads_working) {
		pthread_cond_wait(&thpool_p->threads_all_idle, &thpool_p->thcount_lock);
	}
	pthread_mutex_unlock(&thpool_p->thcount_lock);
}


/* Destroy the threadpool */
void thpool_destroy(thpool_* thpool_p){
	/* No need to destory if it's NULL */
	if (thpool_p == NULL) return ;

	volatile int threads_total = thpool_p->num_threads_alive;

	/* End each thread 's infinite loop */
	threads_keep_alive = 0;

	/* Give one second to kill idle threads */
	double TIMEOUT = 1.0;
	time_t start, end;
	double tpassed = 0.0;
	time (&start);
	while (tpassed < TIMEOUT && thpool_p->num_threads_alive){
		bsem_post_all(thpool_p->jobqueue.has_jobs);
		time (&end);
		tpassed = difftime(end,start);
	}

	/* Poll remaining threads */
	while (thpool_p->num_threads_alive){
		bsem_post_all(thpool_p->jobqueue.has_jobs);
		sleep(1);
	}

	/* Job queue cleanup */
	jobqueue_destroy(&thpool_p->jobqueue);
	/* Deallocs */
	int n;
	for (n=0; n < threads_total; n++){
		thread_destroy(thpool_p->threads[n]);
	}
	free(thpool_p->threads);
	free(thpool_p);
}


/* Pause all threads in threadpool */
void thpool_pause(thpool_* thpool_p) {
	int n;
	for (n=0; n < thpool_p->num_threads_alive; n++){
		pthread_kill(thpool_p->threads[n]->pthread, SIGUSR1);
	}
}


/* Resume all threads in threadpool */
void thpool_resume(thpool_* thpool_p) {
    // resuming a single threadpool hasn't been
    // implemented yet, meanwhile this supresses
    // the warnings
    (void)thpool_p;

	threads_on_hold = 0;
}


int thpool_num_threads_working(thpool_* thpool_p){
	return thpool_p->num_threads_working;
}





/* ============================ THREAD ============================== */

static thread *thread_new (void) {

	thread *t = (thread *) malloc (sizeof (thread));
	if (t) {
		memset (t, 0, sizeof (thread));
		t->thpool_p = NULL;
	}

	return t;

}

static inline void thread_delete (thread *t) { if (t) free (t); }

static int thread_init (threadpool *thpool, thread **thread_p, unsigned int id) {

	int retval = 1;

	if (thpool) {
		*thread_p = thread_new ();

		(*thread_p)->thpool_p = thpool;
		(*thread_p)->id       = id;

		if (!pthread_create (&(*thread_p)->pthread, NULL, (void *)thread_do, (*thread_p))) {
			if (!pthread_detach ((*thread_p)->pthread)) {
				retval = 0;
			}

			else {
				#ifdef CERVER_DEBUG
				cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to detach thread in thpool!");
				#endif
			}
		}

		else {
			#ifdef CERVER_DEBUG
			cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to create thread in thpool!");
			#endif
		}
	}

	return retval;

}

// sets the calling thread on hold
static void thread_hold (int sig_id) {

	(void) sig_id;
	threads_on_hold = 1;
	while (threads_on_hold) sleep (1);

}


/* What each thread is doing
*
* In principle this is an endless loop. The only time this loop gets interuppted is once
* thpool_destroy() is invoked or the program exits.
*
* @param  thread        thread that will run this function
* @return nothing
*/
static void* thread_do(struct thread* thread_p){

	/* Set thread name for profiling and debuging */
	char thread_name[128] = {0};
	sprintf(thread_name, "thread-pool-%d", thread_p->id);

#if defined(__linux__)
	/* Use prctl instead to prevent using _GNU_SOURCE flag and implicit declaration */
	prctl(PR_SET_NAME, thread_name);
#elif defined(__APPLE__) && defined(__MACH__)
	pthread_setname_np(thread_name);
#else
	err("thread_do(): pthread_setname_np is not supported on this system");
#endif

	/* Assure all threads have been created before starting serving */
	thpool_* thpool_p = thread_p->thpool_p;

	/* Register signal handler */
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = thread_hold;
	if (sigaction(SIGUSR1, &act, NULL) == -1) {
		err("thread_do(): cannot handle SIGUSR1");
	}

	/* Mark thread as alive (initialized) */
	pthread_mutex_lock(&thpool_p->thcount_lock);
	thpool_p->num_threads_alive += 1;
	pthread_mutex_unlock(&thpool_p->thcount_lock);

	while(threads_keep_alive){

		bsem_wait(thpool_p->jobqueue.has_jobs);

		if (threads_keep_alive){

			pthread_mutex_lock(&thpool_p->thcount_lock);
			thpool_p->num_threads_working++;
			pthread_mutex_unlock(&thpool_p->thcount_lock);

			/* Read job from queue and execute it */
			void (*func_buff)(void*);
			void*  arg_buff;
			job* job_p = jobqueue_pull(&thpool_p->jobqueue);
			if (job_p) {
				func_buff = job_p->function;
				arg_buff  = job_p->arg;
				func_buff(arg_buff);
				free(job_p);
			}

			pthread_mutex_lock(&thpool_p->thcount_lock);

			thpool_p->num_threads_working--;

			if (!thpool_p->num_threads_working) 
				pthread_cond_signal(&thpool_p->threads_all_idle);

			pthread_mutex_unlock(&thpool_p->thcount_lock);
		}
	}

	pthread_mutex_lock(&thpool_p->thcount_lock);
	thpool_p->num_threads_alive --;
	pthread_mutex_unlock(&thpool_p->thcount_lock);

	return NULL;
}


/* ============================ JOB QUEUE =========================== */

static jobqueue *jobqueue_new (void) {

	jobqueue *job_queue = (jobqueue *) malloc (sizeof (jobqueue));
	if (job_queue) {
		memset (job_queue, 0, sizeof (jobqueue));
		job_queue->front = NULL;
		job_queue->rear = NULL;
		job_queue->has_jobs = (struct bsem *) malloc (sizeof (struct bsem));
		if (!job_queue->has_jobs) {
			free (job_queue);
			job_queue = NULL;
		}
	}

	return job_queue;

}

// creates and initializes a new jobqueue
static jobqueue *jobqueue_init () {

	jobqueue *job_queue = jobqueue_new ();
	if (job_queue) {
		job_queue->len = 0;

		pthread_mutex_init (&(job_queue->rwmutex), NULL);
		// FIXME: allocate bsem there!
		bsem_init (job_queue->has_jobs, 0);
	}

}

/* Clear the queue */
static void jobqueue_clear(jobqueue* jobqueue_p){

	while(jobqueue_p->len){
		free(jobqueue_pull(jobqueue_p));
	}

	jobqueue_p->front = NULL;
	jobqueue_p->rear  = NULL;
	bsem_reset(jobqueue_p->has_jobs);
	jobqueue_p->len = 0;

}


/* Add (allocated) job to queue
 */
static void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob){

	pthread_mutex_lock(&jobqueue_p->rwmutex);
	newjob->prev = NULL;

	switch(jobqueue_p->len){

		case 0:  /* if no jobs in queue */
					jobqueue_p->front = newjob;
					jobqueue_p->rear  = newjob;
					break;

		default: /* if jobs in queue */
					jobqueue_p->rear->prev = newjob;
					jobqueue_p->rear = newjob;

	}
	jobqueue_p->len++;

	bsem_post(jobqueue_p->has_jobs);
	pthread_mutex_unlock(&jobqueue_p->rwmutex);
}


/* Get first job from queue(removes it from queue)
 */
static struct job* jobqueue_pull(jobqueue* jobqueue_p){

	pthread_mutex_lock(&jobqueue_p->rwmutex);
	job* job_p = jobqueue_p->front;

	switch(jobqueue_p->len){

		case 0:  /* if no jobs in queue */
		  			break;

		case 1:  /* if one job in queue */
					jobqueue_p->front = NULL;
					jobqueue_p->rear  = NULL;
					jobqueue_p->len = 0;
					break;

		default: /* if >1 jobs in queue */
					jobqueue_p->front = job_p->prev;
					jobqueue_p->len--;
					/* more than one job in queue -> post it */
					bsem_post(jobqueue_p->has_jobs);

	}

	pthread_mutex_unlock(&jobqueue_p->rwmutex);
	return job_p;
}


// free all queue resources back to the system 
static void jobqueue_delete (jobqueue *job_queue) {

	if (job_queue) {
		jobqueue_clear (job_queue);
		free (job_queue);
	}

}

/* ======================== SYNCHRONISATION ========================= */


/* Init semaphore to 1 or 0 */
static void bsem_init(bsem *bsem_p, int value) {
	if (value < 0 || value > 1) {
		err("bsem_init(): Binary semaphore can take only values 1 or 0");
		exit(1);
	}
	pthread_mutex_init(&(bsem_p->mutex), NULL);
	pthread_cond_init(&(bsem_p->cond), NULL);
	bsem_p->v = value;
}


/* Reset semaphore to 0 */
static void bsem_reset(bsem *bsem_p) {
	bsem_init(bsem_p, 0);
}


/* Post to at least one thread */
static void bsem_post(bsem *bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	bsem_p->v = 1;
	pthread_cond_signal(&bsem_p->cond);
	pthread_mutex_unlock(&bsem_p->mutex);
}


/* Post to all threads */
static void bsem_post_all(bsem *bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	bsem_p->v = 1;
	pthread_cond_broadcast(&bsem_p->cond);
	pthread_mutex_unlock(&bsem_p->mutex);
}


/* Wait on semaphore until semaphore has value 0 */
static void bsem_wait(bsem* bsem_p) {
	pthread_mutex_lock(&bsem_p->mutex);
	while (bsem_p->v != 1) {
		pthread_cond_wait(&bsem_p->cond, &bsem_p->mutex);
	}
	bsem_p->v = 0;
	pthread_mutex_unlock(&bsem_p->mutex);
}
