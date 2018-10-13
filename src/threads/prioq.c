#include "./prioq.h"

/*#include <debug.h>*/
/*#include <stdlib.h>*/
#include "threads/malloc.h"

#ifndef ASSERT
#define ASSERT(x) (void)(0)
#endif

void 
prioq_init(struct prio_queue *pq)
 {
  ASSERT (pq != NULL);

  pq->capacity = PRIOQ_INITIAL_CAPACITY;
  pq->count = (size_t)0;
  int array_length = pq->capacity + 1;
  pq->array = malloc(array_length * sizeof(void *));
}

void
prioq_cleanup(struct prio_queue *pq) 
 {
    ASSERT (pq != NULL);

    free(pq->array);
}

void
prioq_add(struct prio_queue *pq, void *element, prioq_less_func less_func) 
 {
    size_t index, parent;

    if (pq->count == pq->capacity) {
        pq->capacity = pq->capacity * 2 + 1;
        pq->array = realloc(pq->array, (pq->capacity + 1) * sizeof(void *));
    }

    /* add the element to the bottom of the heap */
    index = (++pq->count);

    parent = index >> 1;

    while (index > 1 && less_func(element, pq->array[parent])) {
        pq->array[index] = pq->array[parent];
        index = parent;
        parent = index >> 1;
    }

    pq->array[index] = element;
}

void 
*prioq_peek(struct prio_queue *pq)
 {
    if (pq->count) {
        return pq->array[1];
    }
    return NULL;
}

#define SWAP(a,b,TYPE) { TYPE c = (a); (a) = (b); (b) = (c); }

void 
prioq_remove_first(struct prio_queue *pq, prioq_less_func less_func)
 {
    size_t index = 1;

    if (pq->count == 0) return;
    
    pq->array[1] = pq->array[pq->count--];

    while (index <= pq->count) {
        size_t left = index * 2;
        size_t right = left + 1;
        size_t compare;

        if (left > pq->count) {
            break;
        }

        if (right > pq->count) {
            // if in wrong order
            if (less_func(pq->array[left], pq->array[index])) {
                SWAP(pq->array[left], pq->array[index], void *);
            }
            // safely break here, this is definitely the last level
            break;
        }

        // find smallest element of the two children
        if (less_func(pq->array[left], pq->array[right])) {
            compare = left;
        }
        else {
            compare = right;
        }

        if (!less_func(pq->array[compare], pq->array[index])) {
            // correct order - break!
            break;
        }

        SWAP(pq->array[index], pq->array[compare], void *);
        index = compare;
    }
}
