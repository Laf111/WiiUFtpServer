#include "common.h"

#if !defined(USE_LWP_LOCK) && (defined(__wiiu__) && defined(__WUT__))

#ifndef mutex_t
typedef int mutex_t;
#endif

void __attribute__ ((weak)) _FAT_lock_init(mutex_t *mutex)
{
	return;
}

void __attribute__ ((weak)) _FAT_lock_deinit(mutex_t *mutex)
{
	return;
}

void __attribute__ ((weak)) _FAT_lock(mutex_t *mutex)
{
	return;
}

void __attribute__ ((weak)) _FAT_unlock(mutex_t *mutex)
{
	return;
}

#endif // USE_LWP_LOCK
