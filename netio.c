/*
*	int 	readn(int fd, char* buf, size_t len)
*	int 	readline(int fd, char* buf, size_t len)
*   int     readstring(int fd, char* buf, size_t len)
*	ssize_t writen(int fd, char* buf, ssize_t len)
*   int     recvfrom_msg(int sockfd, void *recvline, int len, unsigned int flags, struct sockaddr *from, int *fromlen)
*
*	char* 	ntostr(struct sockaddr_in *addr, char* buff, int len) buff:where to put string~
*	int 	create_socket(int domain, int type, int protocol) TCP:(AF_INET, SOCK_STREAM, 0)
*	void 	fill_sockaddr_client(struct sockaddr_in *addr, const char* src, int domain, int port) (&servaddr, argv[1], AF_INET, atoi(argv[2]))
*	void 	fill_sockaddr_server(struct sockaddr_in *servaddr, int s_addr, int sin_family, int sin_port) (&servaddr, INADDR_ANY, AF_INET, argv[1])
*	int 	connect_sockaddr(int sockfd, struct sockaddr* addr) client will write this:(sockfd, (struct sockaddr*)&servaddr)
*	int 	bind_sockaddr(int listenfd, struct sockaddr *servaddr)
*	int 	listen_fd(int listenfd, int backlog)
*	void 	accept_connection(int listenfd, struct sockaddr *cliaddr, Handler handler, int flag)
*/

#include "netio.h"

int readn(int fd, char* buf, size_t len){//read n byte, no end of file design
    int n;
    if((n = read(fd, buf, len)) > 0)
        buf[n] = '\0';
    return n;
}
/*
    Read char from given file discriptor into buff upto given len
    "max string length" is len - 1, because the last char is always '\n'
*/
int readline(int fd, char* buf, size_t len){
    int left,n;
    for(left = 0; left < len - 1; left++){
        if((n = read(fd, &buf[left], 1)) < 0){
            return -1;
        }else if(n==0)// have end of file design
            return 0;
        else if(buf[left] == '\n' || buf[left] == EOF){
            buf[left + 1] = '\0';/*buf will have \n*/
            break;
        }
    }

    return left + 1;
}
int readstring(int fd, char* buf, size_t len){
    int left,n;
    for(left = 0; left < len - 1; left++){
        if((n = read(fd, &buf[left], 1)) < 0){
            return -1;   
        }else if(n==0)
            return 0;
        else if(buf[left] == ' ' || buf[left] == '\n' || buf[left] == EOF) {
            buf[left] = '\0';/*buf will not have \n or blank*/
            break;
        }
    }

    return left + 1;
}
ssize_t writen(int fd, char* buf, ssize_t len){
    int n;
    if((n = write(fd, buf, len)) < 0)
        return -1;
    return n;
}

int recvfrom_msg(int sockfd, void *recvline, int len, unsigned int flags, struct sockaddr *from, int *fromlen){
    int get = 0;
    int n;
    time_t ticks1, ticks2;
    ticks1 = time(NULL);
    while(1) {
        if(!get) {
            n = recvfrom(sockfd, recvline, len, flags, from, fromlen);
            sendto(sockfd, "ack", strlen("ack"), 0, from, *(fromlen));
            get++;
        }
        if( ((double)ticks2 - (double)ticks1)>=1 )
            return n;
    }
}

int sendto_msg(int sockfd, const void *sendline, int len, unsigned int flags, const struct sockaddr *to , int tolen){
    int n;
    time_t ticks1, ticks2;
    char recvline[MAXLINE]={};
    while(1){
        sendto(sockfd, sendline, len, flags, to, tolen);
        ticks1 = time(NULL);
        while(1){
            if((n = recvfrom(sockfd, recvline, MAXLINE, MSG_NOBLOCK, (struct sockaddr *)to, &tolen))<0) {
                perror("read");
                exit(1);
            }
            else if(n==0){
                ticks2 = time(NULL);
                if( ((double)ticks2 - (double)ticks1)>=0.25 )
                    break;
            }
            else
                return n;
        }
    }
}

char* ntostr(struct sockaddr_in *addr, char* buff, int len){
    snprintf(buff, len, "%d.%d.%d.%d",
    (int)(addr->sin_addr.s_addr&0xFF),
    (int)((addr->sin_addr.s_addr&0xFF00)>>8),
    (int)((addr->sin_addr.s_addr&0xFF0000)>>16),
    (int)((addr->sin_addr.s_addr&0xFF000000)>>24));

    return buff;
}

int create_socket(int domain, int type, int protocol){
    int sockfd;
    if((sockfd = socket(domain, type, protocol)) < 0){
        perror("netio: socket create error\n");
        exit(1);
    }

    return sockfd;
}

//building servaddr in client.c will use
//void fill_sockaddr_client(xx,xx,xx,xx)
void fill_sockaddr_client(struct sockaddr_in *addr, const char* src, int domain, int port){
    bzero(addr, sizeof(*addr));
    addr->sin_family = domain;//domain should write family!!
    addr->sin_port = htons(port);
    inet_pton(domain, src, &(addr->sin_addr));
}

//building servaddr in server.c will use!!!
void fill_sockaddr_server(struct sockaddr_in *servaddr, int s_addr, int sin_family, int sin_port){
    bzero(servaddr, sizeof(*servaddr));
    servaddr->sin_family = sin_family;
    servaddr->sin_addr.s_addr = htonl(s_addr);//usually : "INADDR_ANY"
    servaddr->sin_port = htons(sin_port);
}
int connect_sockaddr(int sockfd, struct sockaddr* addr){
    int flag;
    if((flag = connect(sockfd, addr, sizeof(*addr))) < 0){
        perror("netio: connect error\n");
        exit(1);
    }

    return flag;
}

int bind_sockaddr(int listenfd, struct sockaddr *servaddr){
    int flag;
    if((flag = bind(listenfd, servaddr, sizeof(*servaddr))) < 0){
        perror("netio: failed to bind socket\n");
        exit(1);
    }
    return flag;
}

int listen_fd(int listenfd, int backlog){
    int flag;
    if((flag = listen(listenfd, backlog)) < 0){
        perror("netio: failed to listen\n");
        exit(1);
    }
    return flag;
}

void accept_connection(int listenfd, struct sockaddr *cliaddr, Handler handler, int flag){
    while(1){
        int clilen = sizeof(*cliaddr);
        int connfd = accept(listenfd, cliaddr, &clilen);
        if(connfd < 0){
            if(errno == EINTR) continue;
            perror("netio: failed to accept\n");
        }else{
            handler(listenfd, connfd, cliaddr);
        }

        if(flag == ACCEPT_ONCE) break;
    }
}
