CFLAGS= -fPIC -Wall -Wextra -O3
LDFLAGS= -shared
TARGET= libmpool.so

all: ${TARGET}

${TARGET}: mpool.o
	$(CC) $(LDFLAGS) -o $@ $^

mpool.o: mpool.c
	$(CC) $(CFLAGS) -c $< -o $@

test: multi-thread-test.c mpool.c
	$(CC) -Wall -Wextra -g $^ -o $@ -lpthread


.PHONY: clean
clean:
	rm -f $(TARGET) mpool.o test
