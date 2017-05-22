# mpool
Small memory pool library for allocating fixed size amounts of memory across threads. 

## Why?
This pool works by allocating a large pool of memory once, then distributing chunks of this memory when requested.
The pool is thread-safe so you may use a single instance of the pool across many threads. By having all the memory
allocated up front, it reduces the amount of time spent allocating/freeing later on. It is also very useful for 
applications where there are a large amount of threads that all need a certain data structure.  

## Installing  
To use this library, simply include mpool.c and mpool.h into the project. Should you want a .so file, simply run `make`.  
   
## API  
The API is very small, consisting of only 6 functions. There are also a few error codes that will be outlined.   

### init_mpool()  
```mpool_error init_mpool (size_t block_size, int32_t capacity, struct mpool** pool);```  

This function _must_ be called to initialize the `struct mpool` before using it. The `block_size` argument taken is 
how large the object is that your allocating, while `capacity` is how many you want to allocate. For example, if 
you wanted a pool of 8 ints, the code would look like:  

```.c
mpool_error err; // Error code returned by functions
struct mpool* pool; 
err = init_mpool(sizeof(int), 8, &pool);
/* Check error value */ 
```  
  
## mpool_alloc()  
```void* mpool_alloc (struct mpool* pool, mpool_error* err); ```  

This function is used to allocate a block of memory from the pool. Note that the `mpool_error` is passed by reference,
and will be set by the function. If you like to not check error return codes, your welcome to pass in `NULL` instead
of a valid pointer, however this is not recommended. __Never__ `free()` the values returned by this function.  

You must also cast the return of this function to the proper type. As above, assuming we have a pool of 8 ints, the 
code would look like this:   
  
```.c
int* i = (int*) mpool_alloc(pool, &err);
/* Check error value */
/* Do stuff with *i */
```    
  
## mpool_dealloc()  
```mpool_error mpool_dealloc (void* item, struct mpool* pool); ```  
  
This function is used to dealloc a block of memory that was allocated with `mpool_alloc()`. Currently there is no checks
on the memory coming back in so you _must_ only give back memory that was alloc'd with the pool. Note that you don't need to 
call this function at the end of the program to free all the blocks, this is done automatically by `free_mpool()`. An
example usage would look like this (assume the variables from the above example are still in use):  
  
```.c
err = mpool_dealloc(i, pool);
/* Check error value */
```  
  
