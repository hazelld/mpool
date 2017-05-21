/* LICENSE */
/* INTRO */


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h> // temp

#ifdef _WIN32
#	warn No multi-threaded support currently
#elif __APPLE__ || __linux__
#	include <pthread.h>
#	define MULTITHREAD 1
#	define LOCK_TYPE pthread_mutex_t
#	define MUTEX_LOCK pthread_mutex_lock
#	define MUTEX_UNLOCK pthread_mutex_unlock
#	define MUTEX_INIT pthread_mutex_init
#else
#	warn Unknown compiler being used.
#endif

typedef enum mpool_error {
	MPOOL_SUCCESS = 0,
	MPOOL_FAILURE,
	MPOOL_ERR_ALLOC,
	MPOOL_ERR_INVALID_SIZE,
	MPOOL_ERR_NULL_ARG,
	MPOOL_ERR_MUTEX,
	MPOOL_ERR_INVALID_REALLOC_SIZE,
	MPOOL_FULL_LIST,
	MPOOL_EMPTY_LIST,
} mpool_error;


struct mpool;


mpool_error init_mpool (size_t block_size, int32_t capacity, struct mpool** pool);

void* mpool_alloc (struct mpool* pool, mpool_error* error);

mpool_error mpool_dealloc (void* item, struct mpool* pool);

mpool_error mpool_realloc (int32_t new_capacity, struct mpool* pool);

mpool_error free_mpool (struct mpool* pool);


