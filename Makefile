LDFLAGS = -O0 -Wall -Wextra $(shell pkg-config --libs -cflags zlib)

firmimg: firmimg.c
	$(CC) firmimg.c -o firmimg $(LDFLAGS)

clean:
	rm -rvf firmimg
