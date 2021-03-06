#include "spinlock.h"
#include <assert.h>

/* This implements a "test and test-and-set lock"
 * to consider cache-coherence.
 * The difference in runtime will be obvious in a test program.*/
void
spin_lock::lock_ttas(void *owner)
{
        volatile uint32_t *lp = &_lock_word;
	do {
            while(*lp);
        } while(!__sync_bool_compare_and_swap_4(lp, 0, 1));
}
void
spin_lock::lock_tas(void *owner)
{
        volatile uint32_t *lp = &_lock_word;
        while(!__sync_bool_compare_and_swap_4(lp, 0, 1));
}

/* The unlock inserts a barrier so that the subsequent loads & previous stores
 * are completed in the right order.
 */
void
spin_lock::unlock(void *owner)
{
    __asm__ __volatile__ ("": : :"memory");
    _lock_word = 0;
}
