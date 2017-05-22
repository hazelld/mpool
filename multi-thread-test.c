#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include "mpool.h"

typedef struct item {
	int i;
	int j;
} item;


/*	These two global arrays are used to store the allocated items. They are global 
 *	so that it is easier to verify that all of them have unique addresses from the
 *	pool.
 */
static const int max_items = 20000;
static item* items1[5000];
static item* items2[5000];
static item* items3[5000];
static item* items4[5000];

static item* all_items[20000];

/* Global pool struct that is used by both threads to alloc from */
struct mpool* pool = NULL;


void init_item (item* i, int n) 
{
	i->i = n;
	i->i = n;
}

/*	This is the function that the new threads are created on. It is used to alloc
 *	memory from the global pool.
 */
void* alloc_items (void* ptr) 
{
	mpool_error err;
	int id = *((int*)ptr);
	for (int i = 0; i < max_items / 4; i++) {
		item* it = (item*) mpool_alloc(pool, &err);
		printf("Thread %d allocated block %p\n", id, it);
		if (err != MPOOL_SUCCESS) {
			fprintf(stderr, "Thread %d mpool_alloc failed with %d\n", id, err);
		}

		if (id == 1)
			items1[i] = it;
		else if (id == 2)
			items2[i] = it;
		else if (id == 3)
			items3[i] = it;
		else if (id == 4)
			items4[i] = it;
		else
			fprintf(stderr, "Unknown thread id %d\n", id);
	}
	return NULL;
}


int check_and_add (item* it) 
{
	static int count = 0;
	all_items[count++] = it;

	for (int i = 0; i < count - 1; i++) {
		if (all_items[i] == it)
			return 1;
	}
	return 0;
}

int main(void) 
{
	/* Initialize the pool */
	mpool_error err = init_mpool(sizeof(item), max_items, &pool);
	assert(err == MPOOL_SUCCESS);

	pthread_t thread1, thread2, thread3, thread4;

	/* Run each thread to allocate memory from the pool */
	int id1 = 1;
	int id2 = 2;
	int id3 = 3;
	int id4 = 4;


	pthread_create(&thread1, NULL, alloc_items, (void*)&id1);
	pthread_create(&thread2, NULL, alloc_items, (void*)&id2);
	pthread_create(&thread3, NULL, alloc_items, (void*)&id3);
	pthread_create(&thread4, NULL, alloc_items, (void*)&id4);
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	pthread_join(thread3, NULL);
	pthread_join(thread4, NULL);

	/* Verify that the array items1 and items2 _don't_ have any matching 
	 * addresses.
	 */
	printf("Testing for matches ... This may be awhile\n");
	for (int i = 0; i < max_items / 4; i++) {
		printf("Checking %d / %d\n", i, max_items / 4);

		if (check_and_add(items1[i]) != 0) 
			printf("Found match!! -- items1[%d] == %p\n", i, items1[i]);
		if (check_and_add(items2[i]) != 0) 
			printf("Found match!! -- items2[%d] == %p\n", i, items2[i]);
		if (check_and_add(items3[i]) != 0) 
			printf("Found match!! -- items3[%d] == %p\n", i, items3[i]);
		if (check_and_add(items4[i]) != 0) 
			printf("Found match!! -- items4[%d] == %p\n", i, items4[i]);
	
	}
	free_mpool(pool);
}
