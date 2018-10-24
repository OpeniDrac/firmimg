CC = gcc
LDFLAGS = -O0 -Wall -Wextra $(shell pkg-config --libs -cflags zlib)

firmimg: firmimg.c
	$(CC) $(LDFLAGS) firmimg.c -o firmimg

clean:
	rm -rvf *.o firmimg
