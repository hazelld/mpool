/*
 * mpool.h --- Public Definitions of a fixed size memory pool 
 *
 * Copyright (c) 2017 William Hazell
 * 
 * Author: William Hazell
 * Date: 05/2017
 * Contact: liam.hazell@gmail.com
 *
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
 *
 */

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

#define MPOOL_MAJOR_VERSION 0
#define MPOOL_MINOR_VERSION 1

/* mpool_error: Various error codes returned by the functions.
 *	
 * For full documentation of what each code means, see the README.md, and 
 * to print the error messages to file stream, see print_mpool_error()
 */
typedef enum mpool_error {
	MPOOL_SUCCESS = 0,
	MPOOL_FAILURE,
	MPOOL_ERR_ALLOC,
	MPOOL_ERR_NULL_ARG,
	MPOOL_ERR_MUTEX,
	MPOOL_ERR_INVALID_REALLOC_SIZE,
	MPOOL_FULL_POOL,
	MPOOL_EMPTY_POOL,
} mpool_error;


/**
 * struct mpool - Main data type used in program, see mpool.c for full def. 
 * Users of library don't need to know any implementation details.
 */
struct mpool;


/**
 * init_mpool() - This function initializes a struct mpool variable. 
 *
 * @block_size: Size of each block needed (ie sizeof(struct))
 * @capacity: Amount of @block_size chunks needed
 * @pool: Pointer to where the struct mpool* should be initialized
 *
 * Returns: MPOOL_SUCCESS means everything was initialized ok, and the pool is 
 * ready to use. Any other return code means that something went wrong and the
 * pool _should not_ be used.
 *
 * This function should always be called prior to using a struct mpool. It sets
 * up everything and allocates the first blob of memory that is the size
 * @block_size x @capacity in bytes.
 *
 * Note that if you pass a pointer to a struct mpool that has already been 
 * allocated, that pointer will be lost. The recommended use is something like:
 *
 * struct mpool* pool;
 * init_mpool(x,y,&pool);
 *
 * You do _not_ have to set the struct mpool* to NULL prior to passing to 
 * init_pool() as we always allocate *@pool inside the function.
 */
mpool_error init_mpool (size_t block_size, int32_t capacity, struct mpool** pool);

/**
 * mpool_alloc() - Get a chunk of memory from the blob.
 *
 * @pool: Pool structure that has been init with init_mpool()
 * @error: The resulting error code from function will be placed here
 *
 * Returns: Pointer to usable piece of memory 
 *
 * This function is used to allocate a piece of memory from within the pool. 
 * It is up to the user to ensure that you are casting the return into the 
 * proper type of pointer. If you have allocated a block_size of 1, then 
 * cast the result of this call to something with a block_size of 2, it may
 * not error as that next byte will most likely be a part of the blob. You will
 * just end up overwriting the memory space of another block that will be 
 * returned later.
 *
 * Never call free on the memory addresses that have been allocated by this 
 * function. If you had the pointer to the first address of the blob, you will
 * free the entire blob, and if you have a pointer to an address within the 
 * blob you will crash the program. If you wish to return the address to the 
 * pool just use mpool_dealloc(). However, this memory will be free'd when 
 * free_mpool() is called so it is not necessary to always call mpool_dealloc()
 *
 * The return of this call must always be cast to the type of pointer you 
 * are trying to allocate. For instance if you have a pool for int* then the
 * call to this function would be:
 * 
 * int* test = (int*) mpool_alloc(pool, &error);
 *
 * Note that while it is recommended that the value of *@error is always 
 * checked after the function call, you can call this function with NULL as 
 * the argument to ignore the error.
 */
void* mpool_alloc (struct mpool* pool, mpool_error* error);


/**
 * mpool_dealloc() - Return a piece of memory to the pool.
 * @item: Address of the item to return 
 * @pool: Pool structure that has been init with init_pool()
 *
 * Returns: MPOOL_SUCCESS if everything worked, else the corresponding error
 * code.
 *
 * This function is used to return a memory address that has been alloc'd from 
 * the mpool_alloc() function. While it is possible to return any memory 
 * address to the pool, don't. This function currently doesn't check if the mem
 * address being returned to the pool is and address that came from the pool.
 * So any address you put in you may end up taking out later, which may cause
 * weird behaviour.
 *
 * Note that you don't need to call mpool_dealloc() on every address allocated 
 * from mpool_alloc() as it is automatically cleaned up with free_mpool().
 */
mpool_error mpool_dealloc (void* item, struct mpool* pool);

/**
 * mpool_realloc() - Make the pool larger than it currently is
 * @new_capacity: How large the new pool should be
 * @pool: Pool structure that has been init with init_pool()
 *
 * Returns: MPOOL_SUCCESS if everything worked, else the corresponding error
 * code.
 * 
 * This function will allocate a new blob of memory to add into the pool. Note
 * that @new_capacity is the _total_ size of the new pool, not how many extra 
 * blocks the pool should add. That means the following must hold, or the
 * function will return an error:
 * @new_capacity > capacity
 *
 * That means the user must keep track of the size of current pool. This info 
 * may be found with the mpool_capacity() function. 
 */
mpool_error mpool_realloc (int32_t new_capacity, struct mpool* pool);

/**
 * mpool_capacity() - Get the capacity of the current pool
 * @pool: struct mpool to check capacity of
 *
 * Returns: Capacity of the pool, if passed NULL or invalid struct mpool, -1
 * is returned.
 *
 * This function is used to check the capactiy of the pool. It is useful for
 * when you need to realloc extra space. For example if you need 5 extra 
 * spaces in the pool you can do:
 *
 * mpool_realloc (mpool_capacity(pool) + 5, pool);
 */
int32_t mpool_capacity (struct mpool* pool);

/**
 * free_mpool() - Free the struct mpool* structure and all related memory
 * @pool: struct pool to free 
 *
 * This function should _always_ be called to clean up the struct mpool. It 
 * should also only be called on struct mpool that have been allocated through
 * the init_mpool() function.
 */
mpool_error free_mpool (struct mpool* pool);


/**
 * print_mpool_error() - Print the error message associated with error code
 * @fh: File handle to print error too
 * @message: Optional message to print prior to error string
 * @err: Error code
 *
 * Note if you don't want to add optional message, simply pass NULL as the arg
 * for message. However fh _can't_ be NULL or this function returns without
 * doing anything.
 */
void print_mpool_error(FILE* fh, char* message, mpool_error err);
