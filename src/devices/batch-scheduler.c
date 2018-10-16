/* Tests cetegorical mutual exclusion with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"
#include "lib/random.h" //generate random numbers
#include "devices/verbose.h" // Change this file to control output

#define BUS_CAPACITY 3
#define SENDER 0
#define RECEIVER 1
#define NORMAL 0
#define HIGH 1

/*
 *	initialize task with direction and priority
 *	call o
 * */
typedef struct {
	int direction;
	int priority;
} task_t;

/* Lock for synchronizing coordinated semaphore operations */
struct lock sync_lock;

/* Semaphore that keeps track of number of running threads */
struct semaphore running_sema;

/* Semaphore that lets main thread wait for all running threads */
struct semaphore main_wait_sema;

/* Semaphore that keeps track of high-priority sender and
 * receiver threads, waiting for a slot */
struct semaphore high_senders_sema;
struct semaphore high_receivers_sema;

/* Condition variable that lets low priority threads wait
 * until all high priority threads are done */
struct condition high_prio_cond;

/* Condition variable that signals to all threads to start */
struct condition start_all_cond;

/* Semaphore that keeps the maximum connections to BUS_CAPACITY */
struct semaphore capacity_sema;

/* Semaphores that keep track of currently sending/receiving tasks */
struct semaphore senders_sema;
struct semaphore receivers_sema;

/* Condition variables that signals:
 * - to senders that no task is receiving
 * - to receivers that no task is sending
 */
struct condition sending_ok_cond;
struct condition receiving_ok_cond;

void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive);

void init_bus(void);
void senderTask(void *);
void receiverTask(void *);
void senderPriorityTask(void *);
void receiverPriorityTask(void *);


void oneTask(task_t task);/*Task requires to use the bus and executes methods below*/
	void getSlot(task_t task); /* task tries to use slot on the bus */
	void transferData(task_t task); /* task processes data on the bus either sending or receiving based on the direction*/
	void leaveSlot(task_t task); /* task release the slot */



/* initializes semaphores */ 
void init_bus(void){ 
    random_init((unsigned int)123456789); 

    lock_init(&sync_lock);
    sema_init(&capacity_sema, BUS_CAPACITY);

    sema_init(&high_senders_sema, 0);
    sema_init(&high_receivers_sema, 0);
    cond_init(&high_prio_cond);
    cond_init(&start_all_cond);

    sema_init(&senders_sema, 0);
    sema_init(&receivers_sema, 0);
    cond_init(&sending_ok_cond);
    cond_init(&receiving_ok_cond);
}

/*
 *  Creates a memory bus sub-system  with num_tasks_send + num_priority_send
 *  sending data to the accelerator and num_task_receive + num_priority_receive tasks
 *  reading data/results from the accelerator.
 *
 *  Every task is represented by its own thread. 
 *  Task requires and gets slot on bus system (1)
 *  process data and the bus (2)
 *  Leave the bus (3).
 */

void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive)
{
    int total_threads = num_tasks_send +
                        num_task_receive +
                        num_priority_send +
                        num_priority_receive;
    char name[16];

    if (total_threads == 0) return;

    sema_init(&running_sema, 0);
    sema_init(&main_wait_sema, 0);

    /* Create threads in random order - gives better testing */

    while (total_threads > 0) {
        unsigned long r = random_ulong() % 4;
        if (r == 0 && num_priority_send > 0) {
            snprintf(name, 16, "send_high_%d", num_priority_send);
            thread_create(name, HIGH, senderPriorityTask, NULL);
            --num_priority_send;
            --total_threads;
        }
        else if (r == 1 && num_priority_receive > 0) {
            snprintf(name, 16, "rec_high_%d", num_priority_receive);
            thread_create(name, HIGH, receiverPriorityTask, NULL);
            --num_priority_receive;
            --total_threads;
        }
        else if (r == 2 && num_tasks_send > 0) {
            snprintf(name, 16, "send_low_%d", num_tasks_send);
            thread_create(name, HIGH, senderTask, NULL);
            --num_tasks_send;
            --total_threads;
        }
        else if (r == 3 && num_task_receive > 0) {
            snprintf(name, 16, "rec_low_%d", num_task_receive);
            thread_create(name, HIGH, receiverTask, NULL);
            --num_task_receive;
            --total_threads;
        }
    }

    thread_yield();
    lock_acquire(&sync_lock);
    cond_broadcast(&start_all_cond, &sync_lock);
    lock_release(&sync_lock);

    /* Wait for all threads to finish */
    sema_down(&main_wait_sema);
    vmsg("Done!");
}

/* Normal task,  sending data to the accelerator */
void senderTask(void *aux UNUSED){
    task_t task = {SENDER, NORMAL};
    oneTask(task);
}

/* High priority task, sending data to the accelerator */
void senderPriorityTask(void *aux UNUSED){
    task_t task = {SENDER, HIGH};
    oneTask(task);
}

/* Normal task, reading data from the accelerator */
void receiverTask(void *aux UNUSED){
    task_t task = {RECEIVER, NORMAL};
    oneTask(task);
}

/* High priority task, reading data from the accelerator */
void receiverPriorityTask(void *aux UNUSED){
    task_t task = {RECEIVER, HIGH};
    oneTask(task);
}

/* abstract task execution*/
void oneTask(task_t task) {
  getSlot(task);
  transferData(task);
  leaveSlot(task);
}


/* task tries to get slot on the bus subsystem */
void getSlot(task_t task) 
{
    sema_up(&running_sema);

    lock_acquire(&sync_lock);
    if (task.priority) {
        if (task.direction == SENDER) {
            sema_up(&high_senders_sema);
        }
        else {
            sema_up(&high_receivers_sema);
        }
        cond_wait(&start_all_cond, &sync_lock);
    }
    else {
        cond_wait(&start_all_cond, &sync_lock);
        if (high_senders_sema.value + high_receivers_sema.value > 0) {
            cond_wait(&high_prio_cond, &sync_lock);
        }
    }
    if (task.direction == SENDER) {
        if (receivers_sema.value > 0) {
            cond_wait(&sending_ok_cond, &sync_lock);
        }
        sema_up(&senders_sema);
    }
    else {
        if (senders_sema.value > 0) {
            cond_wait(&receiving_ok_cond, &sync_lock);
        }
        sema_up(&receivers_sema);
    }
    lock_release(&sync_lock);

    sema_down(&capacity_sema);
}

#define RANDOM_SLEEP_MIN 10
#define RANDOM_SLEEP_MAX 100
#define RANDOM_SLEEP_INTERVAL (RANDOM_SLEEP_MAX - RANDOM_SLEEP_MIN + 1)

/* task processes data on the bus send/receive */
void transferData(task_t task) 
{
    static char *action_verbs[] = { "Sending", "Receiving" };
    unsigned long ticks = random_ulong() % RANDOM_SLEEP_INTERVAL + RANDOM_SLEEP_MIN;
    vmsg("%s %s data for %lu ticks", thread_name(), action_verbs[task.direction], ticks);
    timer_sleep(ticks);
}

/* task releases the slot */
void leaveSlot(task_t task) 
{
    lock_acquire(&sync_lock);
    if (task.priority) {
        if (task.direction == SENDER) {
            sema_down(&high_senders_sema);
        }
        else {
            sema_down(&high_receivers_sema);
        }
        if (high_senders_sema.value + high_receivers_sema.value == 0) {
            cond_broadcast(&high_prio_cond, &sync_lock);
        }
    }

    if (task.direction == SENDER) {
        sema_down(&senders_sema);
        if (sema_try_down(&senders_sema)) {
            sema_up(&senders_sema);
        }
        else {
            cond_broadcast(&receiving_ok_cond, &sync_lock);
        }
    }
    else {
        sema_down(&receivers_sema);
        if (sema_try_down(&receivers_sema)) {
            sema_up(&receivers_sema);
        }
        else {
            cond_broadcast(&sending_ok_cond, &sync_lock);
        }
    }

    sema_down(&running_sema);
    if (running_sema.value == 0) {
        sema_up(&main_wait_sema);
    }

    lock_release(&sync_lock);
    sema_up(&capacity_sema);
}
