CC := gcc
OBJS := socket.o http.o
CFLAGS := -g -D__WORKER__=4 -D__MAX_CONNECTION__=512
LIBS := -lpthread


all: clean $(OBJS)
	$(CC) $(CFLAGS) -o dweb $(OBJS) $(LIBS)

clean:
	rm -rf $(OBJS)

.PHONY: TAGS
TAGS:
	rm -rf TAGS
	find . -iregex ".*\.[ch]" -exec etags -a {} \;
