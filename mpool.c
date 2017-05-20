#include "mpool.h"


/*
 *	This is the main structure used within this library. It defines a block of 
 *	memory that is within the pool. The address it points too will always be 
 *	a valid block of allocated memory.
 *
 *	This is a doubley linked list to allow for fast insertions and deletions, as
 *	that is all we care about. The list will never be traversed, only added too 
 *	and deleted from. When the user allocates memory from this pool, the block
 *	is removed from the front. When the user deallocates memory it is readded 
 *	to the back of the list. This also applies when the user requests more 
 *	memory.
 *
 */
struct _block {
	struct _block* next;
	struct _block* prev;
	void* addr;
};


static mpool_error _create_block (void* addr, struct _block** block) 
{
	if (addr == NULL || block == NULL)
		return MPOOL_ERR_NULL_ARG;

	if (*block == NULL) {
		*block = malloc(sizeof(struct _block));
		if (*block == NULL) return MPOOL_ERR_ALLOC;
	}

	(*block)->next = NULL;
	(*block)->prev = NULL;
	(*block)->addr = addr;
	return MPOOL_SUCCESS;
}

static mpool_error _add_block (struct _block* new_block, struct mpool* pool) 
{

#ifdef PTHREAD
	if (pthread_mutex_lock(&pool->insert_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif

	if (pool->list_size + 1 > pool->capacity)
		return MPOOL_FULL_LIST;

	struct _block* buff = pool->block_list_end;

	/* If the list is empty, make sure we have ptr to start and end */
	if (buff == NULL) {
		pool->block_list_end = new_block;
		pool->block_list_start = new_block;
	} else {
		buff->next = new_block;
		new_block->prev = buff;
		pool->block_list_end = new_block;
	}
	
	pool->list_size++;
#ifdef PTHREAD
	if (pthread_mutex_unlock(&pool->insert_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif

	return MPOOL_SUCCESS;
}

static mpool_error _remove_block (struct _block** block, struct mpool* pool) 
{

#ifdef PTHREAD
	if (pthread_mutex_lock(&pool->remove_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif

	if (pool->block_list_start == NULL)
		return MPOOL_EMPTY_LIST;

	*block = pool->block_list_start;
	pool->block_list_start = (*block)->next;
	pool->block_list_start->prev = NULL;
	pool->list_size--;

#ifdef PTHREAD
	if (pthread_mutex_unlock(&pool->remove_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif
}

mpool_error _partition_blob (struct mpool* pool, int index)
{	
	for (int i = 0; i < pool->blob_sizes[index]; i += pool->block_size) {
		
		/* Because void* pointer arithmatic is undefined, have to cast 
		 * to a complete type, then back to void* 
		 */
		void* ptr = (void*)((char*) pool->blobs[i] + index);
		struct _block* new_block = NULL;
		mpool_error err = _create_block(ptr, &new_block);
		if (err != MPOOL_SUCCESS) return err;

		err = _add_block(new_block, pool);
		if (err != MPOOL_SUCCESS) return err;
	}

	return MPOOL_SUCCESS;
}


mpool_error init_mpool (size_t block_size, int32_t capacity, struct mpool** pool) 
{
	if (pool == NULL)
		return MPOOL_ERR_NULL_ARG;

	*pool = calloc(sizeof(struct mpool));
	if (*pool == NULL)
		return MPOOL_ERR_ALLOC;
	
	(*pool)->block_size = block_size;
	(*pool)->capacity = capacity;
	
#ifdef PTHREAD
	if (pthread_mutex_init(&(*pool)->insert_mutex) != 0)
			return MPOOL_ERR_MUTEX;
	if (pthread_mutex_init(&(*pool)->remove_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif
	
	/*	Because the user may add more space later, we need to keep track of each 
	 *	malloc call that is made to ensure they are all freed later, which is the 
	 *	purpose of the owned_memory field
	 */
	(*pool)->blobs = malloc(sizeof(void*) * ++(*pool)->alloc_count);
	(*pool)->blobs[0] = malloc(block_size * capacity);
	(*pool)->blob_sizes = malloc(sizeof(size_t));
	(*pool)->blob_sizes[0] = block_size * capacity;
	mpool_error err = _partition_blob(*pool, (*pool)->alloc_count);
}
