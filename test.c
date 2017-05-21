#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "mpool.h"


struct test_struct {
	int field1;
	int field2;
};

void init_struct (struct test_struct* ts, int v) 
{
	ts->field1 = v;
	ts->field2 = v;
}

int main(void) 
{
	struct test_struct* data_arr[200];
	struct mpool* pool = NULL;
	mpool_error err;

	err = init_mpool(sizeof(struct test_struct), 100, &pool);
	assert(err == MPOOL_SUCCESS);

	for (int i = 0; i < 50; i++) {
		struct test_struct* data;
		data = (struct test_struct*)mpool_alloc(pool, &err);
		assert(err == MPOOL_SUCCESS);
		data_arr[i] = data;
		init_struct(data_arr[i], i);
		struct test_struct* tes = (struct test_struct*)data_arr[i];
		printf("%d --- %d\n", tes->field1, tes->field2); 
	}

	for (int i = 49; i >= 25; i--) {
		err = mpool_dealloc(data_arr[i], pool);
		assert(err == MPOOL_SUCCESS);
	}

	for (int i = 25; i < 100; i++) {
		struct test_struct* data;
		data = (struct test_struct*)mpool_alloc(pool, &err);
		assert(err == MPOOL_SUCCESS);
		data_arr[i] = data;
		init_struct(data_arr[i], i);
		struct test_struct* tes = data_arr[i];
		printf("%d --- %d\n", tes->field1, tes->field2); 
	}

	for (int i = 0; i < 100; i++) {
		err = mpool_dealloc(data_arr[i], pool);
		assert(err == MPOOL_SUCCESS);
	}

	err = mpool_realloc(200, pool);
	printf("%d\n", err);
	assert(err == MPOOL_SUCCESS);

	for (int i = 0; i < 200; i++) {
		struct test_struct* data;
		data = (struct test_struct*) mpool_alloc(pool, &err);
		assert(err == MPOOL_SUCCESS);
		data_arr[i] = data;
		init_struct(data_arr[i], i);
	}


	for (int i = 0; i < 200; i++) {
		struct test_struct* ts = data_arr[i];
		assert(ts->field1 == i);
		assert(ts->field2 == i);
	}

	free_mpool(pool);

}
