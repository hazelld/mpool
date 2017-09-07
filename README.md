# mpool
Small memory pool library for allocating fixed size amounts of memory across threads. This library is still in it's infancy and
may be prone to errors.

## Why?
This pool works by allocating a large pool of memory once, then distributing chunks of this memory when requested.
The pool is thread-safe so you may use a single instance of the pool across many threads. By having all the memory
allocated up front, it reduces the amount of time spent allocating/freeing later on. It is also very useful for 
applications where there are a large amount of threads that all need a certain data structure.  

## Installing  
To use this library, simply include mpool.c and mpool.h into the project. Should you want a .so file, simply run `make`.  
   
## API  
The API is very small, consisting of only 7 (main) functions. There are also a few error codes that will be outlined.   

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
  
### mpool_alloc()  
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
  
### mpool_dealloc()  
```mpool_error mpool_dealloc (void* item, struct mpool* pool); ```  
  
This function is used to dealloc a block of memory that was allocated with `mpool_alloc()`. Currently there is no checks
on the memory coming back in so you _must_ only give back memory that was alloc'd with the pool. Note that you don't need to 
call this function at the end of the program to free all the blocks, this is done automatically by `free_mpool()`. An
example usage would look like this (assume the variables from the above example are still in use):  
  
```.c
err = mpool_dealloc(i, pool);
/* Check error value */
```  
 
### mpool_realloc()  
```mpool_error mpool_realloc (int32_t new_capacity, struct mpool* pool); ```  
  
This function is used to increase the amount of blocks the pool can hold. The `new_capacity` _must_ be greater than the 
current capacity. It doesn't take the amount of blocks you want to add, rather the total amount of blocks the pool should 
have (much like `realloc()`). This function should be generally avoided, as it defeats the purpose of allocating all the 
memory upfront if you end up having to allocate more later. An example useage would look like this:   

```.c
err = mpool_realloc(16, pool);
/* check error value */
```  

However, generally it is easiest to use in conjunction with the next function.  
  
### mpool_capacity()   
```int32_t mpool_capacity(struct mpool* pool);```  
  
This function returns the current capacity of the pool. It's main purpose is for adding more items with realloc like so:  
   
```.c
err = mpool_realloc(mpool_capacity(pool) + 5, pool); // Allocate 5 extra blocks  
/* check error value */
```  

### free_mpool()  
```mpool_error free_mpool (struct mpool* pool); ```  
  
This function is used to free all memory associated with the `struct mpool`. It should be called whenever the user is finished
with __all__ memory that was allocated from it. Note that once this is called, any memory that was allocated from this 
structure will be freed.  
  
### print_mpool_error()  
```void print_mpool_error(FILE* fh, char* message, mpool_error err);```  
  
This function prints out a plain text error string that corresponds to given error code, to the given file stream. You may
also add an optional message to be prepended to the error string. If you don't want too, simply call with `NULL` as the 
message argument. Example usage:  
  
```.c
mpool_error err = MPOOL_SUCCESS;
print_mpool_error(stdout, NULL, err); // Prints "No Error"  
print_mpool_error(stdout, "Line 10", err); //Prints "Line 10: No Error"
```

## Safe Mode  
As of version `0.1.2` there is now a 'safe' mode that may be activated on the pool. By activating this, some checks will 
be done to prevent someone using the library from making mistakes, at the cost of performance. Currently, there is 
only the one check, but more will be added in later versions.   

The current check being done is too ensure that memory passed back into the pool is valid memory that came
out of the pool. With safe mode off, you are able to 'dealloc' any memory address. Once dealloc'd, you may 
then get this address when you alloc. This means you could end up with an invalid address.   

The API for safe mode are the two following functions:   

```int32_t get_safe_mode(struct mpool* pool); // Returns 0 -> Safe Mode OFF, 1 -> Safe Mode ON ```   

```mpool_error set_safe_mode(struct mpool* pool); // Turn on safe mode```
  
## Error Codes (mpool_error)  
These are the error codes that may be returned from the functions and the associated meanings.  
- __MPOOL_SUCCESS__: Everything worked   
- __MPOOL_FAILURE__: Generic Failure  
- __MPOOL_ERR_ALLOC__: Failed to allocate memory with malloc(). Generally really bad.  
- __MPOOL_ERR_NULL_ARG__: One of the arguments sent to the function was `NULL`    
- __MPOOL_ERR_MUTEX__: One of the functions with the mutex failed    
- __MPOOL_ERR_INVALID_REALLOC_SIZE__: Invalid value sent to `mpool_realloc()` it must be new_capacity > old_capacity  
- __MPOOL_FULL_POOL__: Called `mpool_dealloc()` more times than you have called `mpool_alloc()`    
- __MPOOL_EMPTY_POOL__: No more space left in pool to allocate from. Get more with `mpool_realloc()`  
