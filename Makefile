CC=gcc
CFLAGS=-I. -Wall -Wextra -Werror -O2

array_hashmap.a: array_hashmap.o
	ar rcs array_hashmap.a array_hashmap.o
	
example: example.o array_hashmap.a
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< 
	
clean:
	rm -f *.o *.a example
