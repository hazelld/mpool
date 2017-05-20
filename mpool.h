/* LICENSE */
/* INTRO */


#include <stdlib.h>
#include <stdint.h>


#ifdef _WIN32
#	warn No multi-threaded support currently
#elif __APPLE__
#	define PTHREAD 1
#	include <pthread.h>
#elif __linux__
#	define PTHREAD 1
#	include <pthread.h>
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
	MPOOL_FULL_LIST,
	MPOOL_EMPTY_LIST,
} mpool_error;


struct _block;

struct mpool {
	struct _block* block_list_start;
	struct _block* block_list_end;
	size_t block_size;
	int32_t capacity;
	int32_t list_size;
	void** blobs;
	size_t* blob_sizes;
	int alloc_count;
	
#ifdef PTHREAD
	pthread_mutex_t remove_mutex;
	pthread_mutex_t insert_mutex;
#endif

};


mpool_error init_mpool (size_t block_size, int32_t capacity, struct mpool** pool);

mpool_error mpool_alloc (void** item, struct mpool** pool);

mpool_error mpool_dealloc (void* item, struct mpool** pool);

mpool_error mpool_realloc (int32_t new_capacity, struct mpool** pool);

mpool_error free_mpool (struct mpool* pool);


