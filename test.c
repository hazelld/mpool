#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "mpool.h"


struct test_struct {
	int field1;
	int field2;
};

void init_struct (struct test_struct* ts) 
{
	ts->field1 = 1;
	ts->field2 = 2;
}

int main(void) 
{
	void* data_arr[100];
	struct mpool* pool = NULL;
	mpool_error err;

	err = init_mpool(sizeof(struct test_struct), 100, &pool);
	assert(err == MPOOL_SUCCESS);

	for (int i = 0; i < 50; i++) {
		void* data;
		err = mpool_alloc(&data, pool);
		assert(err == MPOOL_SUCCESS);
		data_arr[i] = data;
		init_struct(data_arr[i]);
		struct test_struct* tes = (struct test_struct*)data_arr[i];
		printf("%d --- %d\n", tes->field1, tes->field2); 
	}

	for (int i = 49; i >= 25; i--) {
		err = mpool_dealloc(data_arr[i], pool);
		assert(err == MPOOL_SUCCESS);
	}

	for (int i = 25; i < 100; i++) {
		void* data;
		err = mpool_alloc(&data, pool);
		assert(err == MPOOL_SUCCESS);
		data_arr[i] = data;
		init_struct(data_arr[i]);
		struct test_struct* tes = (struct test_struct*)data_arr[i];
		printf("%d --- %d\n", tes->field1, tes->field2); 
	}

	for (int i = 0; i < 100; i++) {
		err = mpool_dealloc(data_arr[i], pool);
		assert(err == MPOOL_SUCCESS);
	}

	free_mpool(pool);

}
