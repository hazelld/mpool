/*
 * mpool.c --- Implementation of a fixed size memory pool 
 *
 * Copyright (c) 2017 William Hazell
 * 
 * Author: William Hazell
 * Date: 05/2017
 * Contact: liam.hazell@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "mpool.h"

/* Defining some terminology used throughout the program:
 *
 * blob -> A blob of memory is just the large contiguous block of memory that 
 * has been allocated from the kernel. This occurs when the user initializes 
 * the pool, or reallocs the size. It is equal to block_size * capacity.
 *
 * block -> A block of memory that is within the blob, and is equal in size to 
 * what the user has given (init_mpool() -> @block_size). 
 *
 */


/**
 * struct _block - Linked list holding all the blocks of memory.
 * 
 * @next: Pointer to the next struct _block
 * @addr: Address of the block it points too.
 *
 *
 *	This structure's purpose is to hold the location of a single block within a
 *	blob that has been allocated. When the user alloc's memory, a _block is 
 *	removed from the chain of _blocks, and the address it holds is returned to 
 *	the user. When the user dealloc's an address, it is put inside a currently
 *	empty _block and is added back into the chain.
 *
 *	Note there is a 1:1 ratio between _blocks and the pool's capacity, and this
 *	never changes. These _blocks are malloc'd when the pool is init, and only
 *	freed when free_mpool() is called. Once the _block's memory address is 
 *	given to the user, it is put into the unused_blocks list, until the user
 *	deallocates the space.
 */
struct _block {
	struct _block* next;
	void* addr;
};


/**
 * struct mpool - Main data structure holding everything the pool needs.
 * 
 * @block_list: List of _blocks with free memory addresses
 * @unused_blocks: List of currently unused _blocks
 * @block_size_list: Size of the block list to check if list is full
 * @unused_block_size_list: Size of the unused block list
 * @block_size: Size of each individual block 
 * @capacity: How many blocks the user needs 
 * @blobs: This array holds the chunks of memory that are allocated
 * @blob_sizes: Array of the sizes corresponding to blobs array
 * @alloc_count: How many times memory has been alloc'd (size of blobs array)
 * @block_list_mutex: Holds the lock to @block_list
 * @unused_block_list_mutex: Holds the lock to @unused_block_list
 *
 *
 * 	This structure holds all the needed information for the pool to function.
 * 	When the user uses init_mpool() or mpool_realloc() and memory is needed 
 * 	from the kernel, the "blobs" of memory are stored in the @blobs array. This
 * 	is to keep track of all malloc'd memory so it may be free'd after. This 
 * 	blob is then internally partitioned into _blocks that hold @capacity amount
 * 	of @block_size chunks.
 *
 * 	The lists are locked by a mutex, for both insert and remove operations, and
 * 	are only included if multithreading is enabled.
 *
 */
struct mpool {
	struct _block* block_list;
	struct _block* unused_blocks;
	int32_t block_list_size;
	int32_t unused_block_list_size;
	
	size_t block_size;	
	int32_t capacity;
	
	void** blobs;
	size_t* blob_sizes;
	int alloc_count;
	
#ifdef MULTITHREAD
	LOCK_TYPE block_list_mutex;
	LOCK_TYPE unused_block_list_mutex;
#endif
};


/**
 * _remove_block_list - Remove the first item of list and return it
 * @block: Where to put the removed struct _block
 * @list: Pointer to the first node of list
 *
 * This function removes the first element of any list, and points the value 
 * of *@block to the removed item. Since we are removing the head node, and 
 * there is no seperate structure for the head node, we need a pointer to it, 
 * as it will change.
 *
 * Note that since multiple lists use this function, and each has their own 
 * mutex's, these must be locked prior to calling this function. 
 */
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


/**
 * _insert_block_list - Insert a _block into the head of the list
 * @block: Block to insert
 * @list: List to insert into
 *
 * This function inserts a _block into the front of the list. It functions much
 * like _remove_block_list() in that it needs a pointer to list head pointer, 
 * and also the relevant mutex's must be locked prior to calling the function.
 *
 */
static mpool_error _insert_block_list ( struct _block* block, struct _block** list)
{
	if (block == NULL || list == NULL)
		return MPOOL_ERR_NULL_ARG;

	block->next = *list;
	*list = block;
	return MPOOL_SUCCESS;
}


/**
 * _add_to_unused_list() - Adds a _block into the list of unused _blocks
 * @new_block: Block to add into list
 * @pool: struct mpool* that holds the relevant list
 *
 * This function is used to move a _block into the unused list. The unused list
 * holds all the _blocks that currently are not holding memory addresses from 
 * the pool. This happens when the user calls mpool_alloc(); the address the 
 * _block holds is returned, then the _block is placed into this list. We hold 
 * these as it doesn't make sense to free/malloc each time as the whole point
 * of the library is to cut down on the free/mallocs.
 */
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


/**
 * _remove_from_unused_list() - Remove a _block from the unused list of the pool
 * @block: Location of where to place the struct _block*
 * @pool: struct mpool* that holds the relevant list.
 *
 * This function removes a _block from the unused list so it may be used again.
 * This occurs when a user calls mpool_dealloc() and the address they pass in
 * is then placed into the block returned from this function and put into the 
 * block_list.
 */
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


/**
 * _create_block() - Initialize a struct _block 
 * @addr: Address to hold in the struct _block
 * @block: Where to put the allocated block
 */
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

/**
 * _add_block() - Add a block to the pool's block_list
 * @new_block: Block to add to the list
 * @pool: struct mpool* that holds the block_list
 */
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


/**
 * _remove_block - Remove a block from the pool's block_list
 * @block: Location of where to put _block removed from list
 * @pool: struct mpool* that holds the block_list
 */
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


/**
 * _partition_blob - Split the blob of memory into list of blocks
 * @pool: struct mpool* that holds the raw blob
 * @index: Index of which blob should be partitioned from the @blobs[] array
 *
 * This function converts a raw chunk of allocated memory into a linked list 
 * of _blocks that each hold a pointer to a location within the blob.
 */
mpool_error _partition_blob (struct mpool* pool, int index)
{	
	for (size_t i = 0; i < pool->blob_sizes[index]; i += pool->block_size) {
		
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


/************************************************
 *
 *	Public Functions --- See mpool.h for function
 *	headers.
 *
 ***********************************************/

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


void* mpool_alloc (struct mpool* pool, mpool_error* error) 
{
	struct _block* b;
	mpool_error err = MPOOL_SUCCESS;
	void* item = NULL;

	if (pool == NULL) {
		err = MPOOL_ERR_NULL_ARG;
		goto cleanup;
	}

	if ((err = _remove_block(&b, pool)) != MPOOL_SUCCESS) 
		goto cleanup;
	
	item = b->addr;
	err = _add_to_unused_list(b, pool);

cleanup:
	if (error != NULL)
		*error = err;
	
	return item;
}


mpool_error mpool_dealloc (void* item, struct mpool* pool) 
{
	struct _block* b = NULL;
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
	int index = pool->alloc_count++;
	pool->capacity = new_capacity;

	pool->blobs = realloc(pool->blobs, sizeof(void*) * pool->alloc_count);
	if (pool->blobs == NULL) return MPOOL_ERR_ALLOC;

	pool->blobs[index] = malloc(new_size);
	if (pool->blobs[index] == NULL) return MPOOL_ERR_ALLOC;

	pool->blob_sizes = realloc(pool->blob_sizes, sizeof(size_t) * pool->alloc_count);
	if (pool->blob_sizes == NULL) return MPOOL_ERR_ALLOC;

	pool->blob_sizes[index] = new_size;
	err = _partition_blob(pool, index);
	return err;
}

int32_t mpool_capacity (struct mpool* pool) 
{
	if (pool == NULL) return -1;
	return pool->capacity;
}

mpool_error free_mpool (struct mpool* pool)
{
	if (pool == NULL)
		return MPOOL_ERR_NULL_ARG;

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
	return MPOOL_SUCCESS;
}
