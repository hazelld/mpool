CFLAGS= -fPIC -Wall -Wextra -O3
LDFLAGS= -shared
TARGET= libmpool.so

all: ${TARGET}

${TARGET}: mpool.o
	$(CC) $(LDFLAGS) -o $@ $^

mpool.o: mpool.c
	$(CC) $(CFLAGS) -c $< -o $@


.PHONY: clean
clean:
	rm -f $(TARGET) mpool.o
