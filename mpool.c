#include "mpool.h"


struct _block {
	struct _block* next;
	void* addr;
};

/**/
struct mpool {
	struct _block* block_list;
	struct _block* unused_blocks;
	size_t block_size;
	
	int32_t capacity;
	int32_t block_list_size;
	int32_t unused_block_list_size;
	
	void** blobs;
	size_t* blob_sizes;
	int alloc_count;
	
#ifdef MULTITHREAD
	LOCK_TYPE block_list_mutex;
	LOCK_TYPE unused_block_list_mutex;
#endif
};

static mpool_error _remove_block_list (struct _block** block, struct _block** list) 
{
	if (block == NULL || list == NULL)
		return MPOOL_ERR_NULL_ARG;
	if (*list == NULL)
		return MPOOL_EMPTY_LIST;

	*block = *list;
	*list = (*block)->next;
	return MPOOL_SUCCESS; 
}

static mpool_error _insert_block_list ( struct _block* block, struct _block** list)
{
	if (block == NULL || list == NULL)
		return MPOOL_ERR_NULL_ARG;

	block->next = *list;
	*list = block;
	return MPOOL_SUCCESS;
}


static mpool_error _add_to_unused_list(struct _block* new_block, struct mpool* pool) 
{

#ifdef MULTITHREAD
	if (MUTEX_LOCK(&pool->unused_block_list_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif
	
	mpool_error err = _insert_block_list(new_block, &pool->unused_blocks);

#ifdef MULTITHREAD
	if (MUTEX_UNLOCK(&pool->unused_block_list_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif

	if (err == MPOOL_SUCCESS)
		pool->unused_block_list_size++;

	return MPOOL_SUCCESS;
}

static mpool_error _remove_from_unused_list(struct _block** block, struct mpool* pool)
{

#ifdef MULTITHREAD
	if (MUTEX_LOCK(&pool->unused_block_list_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif

	mpool_error err = _remove_block_list(block, &pool->unused_blocks);

#ifdef MULTITHREAD
	if (MUTEX_UNLOCK(&pool->unused_block_list_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif
	
	if (err == MPOOL_SUCCESS)
		pool->unused_block_list_size--;
	return MPOOL_SUCCESS;
}


static mpool_error _create_block (void* addr, struct _block** block) 
{
	if (addr == NULL || block == NULL)
		return MPOOL_ERR_NULL_ARG;

	if (*block == NULL) {
		*block = malloc(sizeof(struct _block));
		if (*block == NULL) return MPOOL_ERR_ALLOC;
	}

	(*block)->next = NULL;
	(*block)->addr = addr;
	return MPOOL_SUCCESS;
}

static mpool_error _add_block (struct _block* new_block, struct mpool* pool) 
{
	if (pool->block_list_size + 1 > pool->capacity)
		return MPOOL_FULL_LIST;

#ifdef MULTITHREAD
	if (MUTEX_LOCK(&pool->block_list_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif
	
	mpool_error err = _insert_block_list(new_block, &pool->block_list);

#ifdef MULTITHREAD
	if (MUTEX_UNLOCK(&pool->block_list_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif

	if (err == MPOOL_SUCCESS)
		pool->block_list_size++;
	return err;
}

static mpool_error _remove_block (struct _block** block, struct mpool* pool) 
{
	if (block == NULL || pool == NULL)
		return MPOOL_ERR_NULL_ARG;

	if (pool->block_list == NULL)
		return MPOOL_EMPTY_LIST;

#ifdef MULTITHREAD
	if (MUTEX_LOCK(&pool->block_list_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif

	mpool_error err = _remove_block_list(block, &pool->block_list);

#ifdef MULTITHREAD
	if (MUTEX_UNLOCK(&pool->block_list_mutex) != 0)
		return MPOOL_ERR_MUTEX;
#endif
	
	if (err == MPOOL_SUCCESS)
		pool->block_list_size--;
	return err;
}

mpool_error _partition_blob (struct mpool* pool, int index)
{	
	for (int i = 0; i < pool->blob_sizes[index]; i += pool->block_size) {
		
		/* Because void* pointer arithmatic is undefined, have to cast 
		 * to a complete type, then back to void* 
		 */
		void* ptr = (void*)((char*) pool->blobs[index] + i);
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

	*pool = calloc(1, sizeof(struct mpool));
	if (*pool == NULL)
		return MPOOL_ERR_ALLOC;
	
	(*pool)->block_size = block_size;
	(*pool)->capacity = capacity;
	
#ifdef MULTITHREAD
	if (MUTEX_INIT(&(*pool)->block_list_mutex, NULL) != 0)
			return MPOOL_ERR_MUTEX;
	if (MUTEX_INIT(&(*pool)->unused_block_list_mutex, NULL) != 0)
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
	mpool_error err = _partition_blob(*pool, (*pool)->alloc_count - 1);
	return err;
}


mpool_error mpool_alloc (void** item, struct mpool* pool) 
{
	struct _block* b;
	mpool_error err = MPOOL_SUCCESS;
	
	if (item == NULL || pool == NULL) 
		return MPOOL_ERR_NULL_ARG;

	if ((err = _remove_block(&b, pool)) != MPOOL_SUCCESS)
		return err;
	
	*item = b->addr;
	err = _add_to_unused_list(b, pool);
	return err;
}


mpool_error mpool_dealloc (void* item, struct mpool* pool) 
{
	struct _block* b;
	mpool_error err = MPOOL_SUCCESS;

	if (item == NULL || pool == NULL) 
		return MPOOL_ERR_NULL_ARG;
	
	if ((err = _remove_from_unused_list(&b, pool)) != MPOOL_SUCCESS) 
		return err;
	
	b->addr = item;
	err = _add_block(b, pool);
	return err;
}


mpool_error mpool_realloc (int32_t new_capacity, struct mpool* pool) 
{
	mpool_error err = MPOOL_SUCCESS;

	if (pool == NULL) 
		return MPOOL_ERR_NULL_ARG;
	if (pool->capacity >= new_capacity)
		return MPOOL_ERR_INVALID_REALLOC_SIZE;
	
	int32_t extra = new_capacity - pool->capacity;
	size_t new_size = extra * pool->block_size;
	
	pool->blobs = realloc(pool->blobs, sizeof(void*) * ++pool->alloc_count);
	if (pool->blobs == NULL) return MPOOL_ERR_ALLOC;

	pool->blobs[pool->alloc_count - 1] = malloc(new_size);
	if (pool->blobs[pool->alloc_count - 1] == NULL) return MPOOL_ERR_ALLOC;

	pool->blob_sizes = realloc(pool->blob_sizes, sizeof(size_t) * pool->alloc_count);
	if (pool->blob_sizes == NULL) return MPOOL_ERR_ALLOC;

	pool->blob_sizes[pool->alloc_count] = new_size;
	err = _partition_blob(pool, pool->alloc_count);
	return err;
}


mpool_error free_mpool (struct mpool* pool)
{
	struct _block* buff;
	struct _block* b = pool->block_list;
	
	while (b) {
		buff = b->next;
		free(b);
		b = buff;
	}
	
	b = pool->unused_blocks;
	while (b) {
		buff = b->next;
		free(b);
		b = buff;
	}

	for (int i = 0; i < pool->alloc_count; i++) 
		free(pool->blobs[i]);
	free(pool->blobs);
	free(pool->blob_sizes);
	free(pool);
}
