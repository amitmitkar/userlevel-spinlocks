#ifndef __USER_SPIN_LOCK_H__
#define __USER_SPIN_LOCK_H__

#include <stdint.h>

class spin_lock {
 //   char pad[ 128 - sizeof(uint32_t) - sizeof(void*)];
    volatile uint32_t _lock_word;
    void    *_owner;
//    char pad1[ 128 - sizeof(uint32_t) - sizeof(void*)];
public:
    explicit spin_lock():_lock_word(0), _owner(0){;}
    void lock_tas(void *owner);
    void lock_ttas(void *owner);
    void unlock(void *owner);
};

#endif /*__USER_SPIN_LOCK_H__*/
