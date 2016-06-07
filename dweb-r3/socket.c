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
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "socket.h"
#include "http.h"

#define CR '\r'
#define LF '\n'

#define MAX 4096

extern int http_parser(char *, int *);
extern void pipe_read(FILE *, struct http_operations *);

struct http_io {
	struct http_operations *ops;
	pthread_t thread_id;
	int client_sockfd;
};

static void* http_io_thread(void *ptr)
{
#define _READ_TIMES   20
    struct http_io *io = (struct http_io*)ptr;
    int ret;
    int http_get = 0;
    char buf[MAX];
    int i;

    for (i = 0; i<_READ_TIMES; i++) {
        printf("ops->read\n");
	ret = io->ops->read(io->client_sockfd, buf);
	printf("client: %s [%d]\n", buf, ret);
	http_parser(buf, &http_get);
	if (buf[0] == CR && buf[1] == LF)
	    break;
    }
    
    if (http_get) {
        write(io->client_sockfd, "Hello\n", 6);
	write(io->client_sockfd, "Everybody~\n", 11);
    }

    shutdown(io->client_sockfd, SHUT_RDWR);
    close(io->client_sockfd);

    return http_get > 0 ? (void*)0 : (void*)-1;
}


static int
check_thread_status(struct http_io *io, int thread_count)
{
    int i;
    int ret;
    int join_count = 0;
    void *thread_err;

    for (i = 0; i < thread_count; i++) {
        ret = pthread_join(io[i].thread_id, &thread_err);
		
	if (ret == 0 && (int)thread_err == 0) {
	    join_count++;
	    continue;
	}

	/* Detail error */
	switch ((int)thread_err) {
	case EDEADLK:
		printf("A deadlock was detected\n");
		break;
		 
	case EINVAL:
		printf("1) thread is not a joinable thread.\n");
		printf("2) Another thread is already waiting to join with this thread.\n");
		break;
				    
	case ESRCH:
		printf("No thread with the ID thread could be found.\n");
		break;
	}
    }
    
    return join_count;
}


/* supporting functions */
static int socket_listen(struct http_operations *ops)
{
    struct socket_data *priv = (struct socket_data *)ops->priv;
    int sockfd = priv->sockfd;
    int client_sockfd;
    struct sockaddr_in client_addr;
    int len = priv->len;
    pthread_t id;
    
#define THREAD_COUNT   (__MAX_CONNECTION__ / __WORKER__)
    struct http_io io[THREAD_COUNT];
    int thread_idx = 0;
    

    /* Start Web server. */
    while(1) {
        listen(sockfd, THREAD_COUNT);

	client_sockfd = accept(sockfd, &client_addr, &len);
	    
	if (client_sockfd == -1) {
	    perror("accept:");
	    continue;
	}
	    
	io[thread_idx].ops = ops;
	io[thread_idx].client_sockfd = client_sockfd;
	pthread_create(&io[thread_idx].thread_id,
		       NULL,
		       &http_io_thread,
		       (void*)(io + thread_idx));
	++thread_idx;
	
	if (thread_idx < THREAD_COUNT) continue;
	
	printf("Serviced request : %d clients\n", check_thread_status(io, THREAD_COUNT));
	
	/* Reset thread index */
	thread_idx = 0;
    }
    
    return 0;
}


/* callback functions */
int http_write(struct http_operations *ops, char *buf, int n)
{
    struct socket_data *priv = (struct socket_data *)ops->priv;
    int fd = priv->client_sockfd;

    write(fd, buf, n);
}

int http_read(int fd, char *netread)
{
    int len;
    char readch;
    ssize_t n;

    len = 0;
    netread[0] = CR;
    netread[1] = LF;

    while (len < MAX) {
	n = read(fd, &readch, 1);
	if (n <= 0)
	    break;

	netread[len++] = readch;
	if (readch == LF)
	    break;
    }

    netread[len] = '\0';

    return len;
}

int http_open(struct http_operations *ops)
{
    int sockfd;
    int len;
    int i;
    pid_t parent, child;
    struct sockaddr_in server_addr;
    struct socket_data *priv = ops->priv;

    printf("in ops->open\n");
    /* 1. Create a socket. */
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
	perror("socket:");
	return -1;
    }

    /* 2. Bind an address to the socket. */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, &server_addr, sizeof(struct sockaddr_in)) < 0) {
	perror("bind:");
	return -1;
    }
    len = sizeof(struct sockaddr_in);

    priv->sockfd = sockfd;
    priv->len = len;

    /* 3. Fork child processes */
    parent = getpid();
    printf("Parent process \t[%d]\n", parent);
    
    for (i=0; i<__WORKER__; i++) {
	child = fork();
	if (child == 0) break;
	printf("Child process-%d \t[%d]\n", i+1, (int)child);
    }

    if (getpid() == parent) return 0;
    printf("socket:socket_listen\n");
    socket_listen(ops);
}

void http_close(struct http_operations *ops)
{
    struct socket_data *priv = (struct socket_data *)ops->priv;
    int sockfd =  priv->sockfd;

    close(sockfd);
    exit(EXIT_SUCCESS);
}

struct http_operations http_ops =
{
	open:	http_open,
	read:	http_read,
	write:	http_write,
	close:	http_close,
};

int main(int argc, char *argv[])
{
    int child_status;
    http_ops.priv = (struct socket_data *)malloc(sizeof(struct socket_data));  
    http_register(&http_ops, SOCKET_OPS);
    
    while (1) {
        wait(&child_status);
	sleep(1);
    }
}
