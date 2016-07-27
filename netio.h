#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <time.h>

#define MAXLINE 2048
#define ACCEPT_ONCE 0
#define ACCEPT_FOREVER 1
#define MSG_NOBLOCK 0x01

typedef void (*Handler) (int, int, struct sockaddr*);
typedef struct sockaddr SA;

int readn(int, char*, size_t);
int readline(int, char*, size_t);
int readstring(int, char* , size_t);
ssize_t writen(int, char*, ssize_t);

int recvfrom_msg(int sockfd, void *recvline, int len, unsigned int flags, struct sockaddr *from, int *fromlen);
int sendto_msg(int sockfd, const void *sendline, int len, unsigned int flags, const struct sockaddr *to , int tolen);

int create_socket(int domain, int type, int protocol);
void fill_sockaddr_client(struct sockaddr_in *addr, const char* src, int domain, int port);
void fill_sockaddr_server(struct sockaddr_in *servaddr, int s_addr, int sin_family, int sin_port);
int connect_sockaddr(int sockfd, struct sockaddr* addr);
int bind_sockaddr(int listenfd, struct sockaddr *servaddr);
int listen_fd(int listenfd, int backlog);
void accept_connection(int listenfd, struct sockaddr *cliaddr, Handler handler, int flag);
char* ntostr(struct sockaddr_in *addr, char* buff, int len);

