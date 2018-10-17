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
static struct lock sync_lock;

/* Variables for counting number of runnings tasks of different categories */
static int senders_running = 0;
static int receivers_running = 0;
static int high_priority_running = 0;

/* Variables for counting number of waiting tasks of different categories */
static int high_prio_senders_waiting = 0;
static int high_prio_receivers_waiting = 0;
static int low_prio_senders_waiting = 0;
static int low_prio_receivers_waiting = 0;

/* Condition variables for waiting tasks to use */
static struct condition high_prio_senders_cond;
static struct condition high_prio_receivers_cond;
static struct condition low_prio_senders_cond;
static struct condition low_prio_receivers_cond;

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

    /* Initialize condition variables */
    cond_init(&high_prio_senders_cond);
    cond_init(&high_prio_receivers_cond);
    cond_init(&low_prio_senders_cond);
    cond_init(&low_prio_receivers_cond);
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
    char name[16];

    /* Create threads in random order - gives better testing */
    while (num_tasks_send + num_task_receive +
           num_priority_send + num_priority_receive > 0) {
        unsigned long r = random_ulong() % 4;
        if (r == 0 && num_priority_send > 0) {
            snprintf(name, 16, "send_high_%d", num_priority_send);
            thread_create(name, HIGH, senderPriorityTask, NULL);
            --num_priority_send;
        }
        else if (r == 1 && num_priority_receive > 0) {
            snprintf(name, 16, "rec_high_%d", num_priority_receive);
            thread_create(name, HIGH, receiverPriorityTask, NULL);
            --num_priority_receive;
        }
        else if (r == 2 && num_tasks_send > 0) {
            snprintf(name, 16, "send_low_%d", num_tasks_send);
            thread_create(name, HIGH, senderTask, NULL);
            --num_tasks_send;
        }
        else if (r == 3 && num_task_receive > 0) {
            snprintf(name, 16, "rec_low_%d", num_task_receive);
            thread_create(name, HIGH, receiverTask, NULL);
            --num_task_receive;
        }
    }

    /* Repeated sleep until all tasks are done */
    bool waiting = true;
    while (waiting) {
        timer_sleep(100);

        lock_acquire(&sync_lock);
        if (senders_running +
            receivers_running +
            high_priority_running +
            high_prio_senders_waiting +
            high_prio_receivers_waiting +
            low_prio_senders_waiting +
            low_prio_receivers_waiting
            == 0) {
                waiting = false;
            }
        lock_release(&sync_lock);
    }

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
    bool waiting;
    lock_acquire(&sync_lock);

    do {
        int total_running = senders_running + receivers_running;
        bool full = total_running >= BUS_CAPACITY;

        if (task.priority == HIGH && task.direction == SENDER && (receivers_running > 0 || full)) {
            /* High priority sender only waits if bus is currently receiving or full */
            high_prio_senders_waiting++;
            cond_wait(&high_prio_senders_cond, &sync_lock);
            high_prio_senders_waiting--;
            waiting = true;
        }
        else if (task.priority == HIGH && task.direction == RECEIVER && (senders_running > 0 || full)) {
            /* High priority receiver only waits if bus is currently sending or full */
            high_prio_receivers_waiting++;
            cond_wait(&high_prio_receivers_cond, &sync_lock);
            high_prio_receivers_waiting--;
            waiting = true;
        }
        else if (task.priority == NORMAL && task.direction == SENDER && (receivers_running > 0 || high_priority_running > 0 || high_prio_senders_waiting > 0 || full)) {
            /* Low priority sender waits if bus is currently receiving, if any high priority task is running,
             * any high priority sender is waiting, or if the bus is full */
            low_prio_senders_waiting++;
            cond_wait(&low_prio_senders_cond, &sync_lock);
            low_prio_senders_waiting--;
            waiting = true;
        }
        else if (task.priority == NORMAL && task.direction == RECEIVER && (senders_running > 0 || high_priority_running > 0 || high_prio_receivers_waiting > 0 || full)) {
            /* Low priority receiver waits if bus is currently sending, if any high priority task is running,
             * any high priority receiver is waiting, or if the bus is full */
            low_prio_receivers_waiting++;
            cond_wait(&low_prio_receivers_cond, &sync_lock);
            low_prio_receivers_waiting--;
            waiting = true;
        }
        else {
            waiting = false;
        }
    } while (waiting);

    if (task.priority == HIGH) {
        high_priority_running++;
    }
    if (task.direction == SENDER) {
        senders_running++;
    }
    if (task.direction == RECEIVER) {
        receivers_running++;
    }

    lock_release(&sync_lock);
    vmsg("%s starting at %lu", thread_name(), timer_ticks());
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
    vmsg("%s stopping at %lu", thread_name(), timer_ticks());
    lock_acquire(&sync_lock);

    if (task.priority) {
        high_priority_running--;
    }
    if (task.direction == SENDER) {
        senders_running--;
    }
    else {
        receivers_running--;
    }
    
    vmsg("%s Running H=%d S=%d R=%d Waiting HS=%d HR=%d LS=%d LR=%d",
        thread_name(),
        high_priority_running, senders_running, receivers_running,
        high_prio_senders_waiting, high_prio_receivers_waiting,
        low_prio_senders_waiting, low_prio_receivers_waiting);

    if (task.direction == SENDER && high_prio_senders_waiting > 0) {
        /* Currently sending - there are high priority senders waiting */
        cond_signal(&high_prio_senders_cond, &sync_lock);
    }
    else if (task.direction == RECEIVER && high_prio_receivers_waiting > 0) {
        /* Currently receiving - there are high priority receivers waiting */
        cond_signal(&high_prio_receivers_cond, &sync_lock);
    }
    else if (high_prio_senders_waiting > 0) {
        /* There are high priority senders waiting */
        cond_broadcast(&high_prio_senders_cond, &sync_lock);
    }
    else if (high_prio_receivers_waiting > 0) {
        /* There are high priority receivers waiting */
        cond_broadcast(&high_prio_receivers_cond, &sync_lock);
    }
    else if (task.direction == SENDER && low_prio_senders_waiting > 0) {
        /* Currently sending - there are senders waiting */
        cond_signal(&low_prio_senders_cond, &sync_lock);
    }
    else if (task.direction == RECEIVER && low_prio_receivers_waiting > 0) {
        /* Currently receiving - there are receivers waiting */
        cond_signal(&low_prio_receivers_cond, &sync_lock);
    }
    else if (low_prio_senders_waiting > 0) {
        /* There are senders waiting */
        cond_broadcast(&low_prio_senders_cond, &sync_lock);
    }
    else if (low_prio_receivers_waiting > 0) {
        /* There are receivers waiting */
        cond_broadcast(&low_prio_receivers_cond, &sync_lock);
    }

    lock_release(&sync_lock);
}
