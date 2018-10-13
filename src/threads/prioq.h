#ifndef THREADS_PRIOQ_H
#define THREADS_PRIOQ_H

#include <stddef.h>
#ifndef bool
typedef int bool;
#endif

/* Priority queue */

struct prio_queue
  {
    size_t capacity;
    size_t count;
    void **array;
  };

#define PRIOQ_INITIAL_CAPACITY 127

typedef bool prioq_less_func(const void *left, const void *right);

void prioq_init(struct prio_queue *);
void prioq_cleanup(struct prio_queue *);
void prioq_add(struct prio_queue *, void *, prioq_less_func);
void *prioq_peek(struct prio_queue *);
void prioq_remove_first(struct prio_queue *, prioq_less_func);

#define prioq_peek_type(PQ,TYPE) (TYPE *)(prioq_peek(PQ))

#endif /* threads/prioq.h */
