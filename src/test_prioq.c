#include "threads/prioq.h"
#include <stdio.h>

struct test_elem {
    int value;
};

#define CELEM(x) ((const struct test_elem *)(x))

bool test_elem_less(const void *left, const void *right) {
    return (CELEM(left)->value < CELEM(right)->value);
}

int main(int argc, char *argv[]) {
    struct prio_queue pq;
    prioq_init(&pq);

    struct test_elem one   = { 1 };
    struct test_elem two   = { 2 };
    struct test_elem three = { 3 };
    struct test_elem four  = { 4 };
    struct test_elem five  = { 5 };
    struct test_elem six   = { 6 };
    struct test_elem seven = { 7 };
    struct test_elem eight = { 8 };
    struct test_elem nine  = { 9 };

    prioq_add(&pq, &three, test_elem_less);
    prioq_add(&pq, &seven, test_elem_less);
    prioq_add(&pq, &eight, test_elem_less);
    prioq_add(&pq, &two,   test_elem_less);
    prioq_add(&pq, &four,  test_elem_less);
    prioq_add(&pq, &nine,  test_elem_less);
    prioq_add(&pq, &five,  test_elem_less);
    prioq_add(&pq, &one,   test_elem_less);
    prioq_add(&pq, &six,   test_elem_less);

    struct test_elem *elem;
    while (elem = prioq_peek_type(&pq, struct test_elem)) {
        printf("%d\n", elem->value);
        prioq_remove_first(&pq, test_elem_less);
    }
}
