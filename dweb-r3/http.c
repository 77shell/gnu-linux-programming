/*
   web.c,
   a dirty embedded Web server.
*/
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "http.h"

#define CR '\r'
#define LF '\n'

static struct http_operations *http_ops[MAX_OPS];


int http_parser(char *s, int *http_get)
{
    if (s[0] == CR && s[1] == LF)
	return -1;
    else if (strncmp(s, "GET", 3) == 0)
        *http_get = 1;
    return 0;
}

/*
 * Read pipe.
 */
void pipe_read(FILE *stream, struct http_operations *ops)
{
   char buffer[1024];

   while (!feof(stream) && !ferror(stream) &&
          fgets(buffer, sizeof(buffer), stream) != NULL)
	   ops->write(ops, buffer, strlen(buffer));
}


static void* http_main(int n)
{
    struct http_operations *ops;
    int c;
    char *quit_msg = "Press 'q' to quit~";

    ops = (struct http_operations *)http_data[n].fops;

    /* blocking open */
    printf("ops->open\n");
    if (ops == NULL)
        printf("ops = NULL\n");
    if (ops->open) {
	ops->open(ops);
    } else {
	printf("ops->open = NULL\n");
    }

    printf("%s\n", quit_msg);
    while (c = getchar()) {
        if (c == 'q')
		break;
	else
		printf("%s\n", quit_msg);
	
	sleep(1);
    }
	
    printf("exit http_main\n");
    ops->close(ops);
    return NULL;
}

/* API implementation */
int http_register(struct http_operations *ops, int opsno)
{
   	pthread_t thread_id1;

	switch (opsno) {
		case SOCKET_OPS:
			   http_data[SOCKET_OPS].fops = ops;
			   pthread_create(&thread_id1, NULL, &http_main, SOCKET_OPS);
			break;
		case FILE_OPS:
			http_ops[FILE_OPS] = 
			       (struct http_operations *)ops;
			break;
		default:
			printf("error\n");
			return -1;
	}
	printf("exit http_register\n");

	return 0;
}
