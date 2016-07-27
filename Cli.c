#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <time.h> 
#include <netinet/in.h> 
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "pthread.h"
#include "netio.h"
#define MAXLINE 2048

/*global for both thread to access*/
static int sockfd;
static int udpsockfd;
static struct sockaddr* ptalkaddr;
static socklen_t talklen;
static const struct sockaddr* pudpaddr;
static socklen_t udplen;
static FILE *fp;

int flag = 0;//avoid duplicated welcome message
char ID[20]={};//know who am I and tell receiver I am sending him cutsize

char returnstring[MAXLINE]={};//tell client what to do string
void write_returnstring(int sockfd, char *buff){
	memset(returnstring, 0, MAXLINE);
	strcat(returnstring, buff);
	write(sockfd, returnstring, MAXLINE);
}
void sendto_returnstring(int udpsockfd, char *buff, struct sockaddr* pcliaddr, socklen_t clilen){
	memset(returnstring, 0, MAXLINE);
	strcat(returnstring, buff);
	sendto(udpsockfd, returnstring, MAXLINE, 0, pcliaddr, clilen);
}
char filename[50][20]={};//filenum maximum : 50, filename size : 20
void write_filelist(int sockfd){
	int x;
	int filenum = 0;
	DIR *d;
	struct dirent *dir;
	d = opendir(".");
	memset(filename, 0, sizeof(filename));
	while( (dir = readdir(d)) != NULL ){
		// '.':invisible file, '~':invisible file, '\n':directory
		if(dir->d_name[0]!='.'&&dir->d_name[strlen(dir->d_name)-1]!='~'&&dir->d_name[strlen(dir->d_name)-1]!='\n'){
			strcat(filename[filenum], dir->d_name);
			filenum++;
		}
	}
	write(sockfd, &filenum, sizeof(filenum));//tell server howmany files i have
	for(x=0; x<filenum; x++)
		write(sockfd, filename[x], 20);
	closedir(d);
}

void* copyto(void *arg){/*no input argument*/ /*void *arg will get NULL pointer*/
	char sendline[MAXLINE];
	
	while (1){ //(FD_ISSET(fileno(fp), &rset))
		/*welcome message and refresh filelist*/
		if(!flag) {
			write_returnstring(sockfd, "wantapm");
			write_filelist(sockfd);
		}
		flag = 1;

		if(fgets(sendline, MAXLINE, fp)==NULL)
			break;
		write(sockfd, sendline, MAXLINE);

		/*registration*/
		if(sendline[0]=='R'&&sendline[1]!='F'){
			printf("Choose your ID: ");
			fgets(sendline, MAXLINE, fp);
			write(sockfd, sendline, 20);
			printf("Create a password: ");
			fgets(sendline, MAXLINE, fp);
			write(sockfd, sendline, 20);
			flag = 0;
		}
		else if(sendline[0]=='T'){
			char talkline[MAXLINE]={};
			char talktoID[20]={};
			printf("who: ");
			fgets(talktoID, MAXLINE, fp);
			talktoID[strlen(talktoID)-1]='\0';
			write(sockfd, talktoID, 20);
			sleep(1);
			printf("START TO TALK WITH %s:\n", talktoID);
			while(fgets(sendline, MAXLINE, fp)!=NULL){
				memset(talkline, 0, sizeof(talkline));
				sendto_returnstring(udpsockfd, "talk hello", ptalkaddr, talklen);
				sprintf(talkline, "%s: %s", talktoID, sendline);
				sendto(udpsockfd, talkline, MAXLINE, 0, ptalkaddr, talklen);
				printf("%s\n", talkline);
			}
			flag=0;sleep(1);
		}
		/*login*/
		else if(sendline[0]=='L'){
			printf("ID: ");
			fgets(ID, MAXLINE, fp);
			write(sockfd, ID, 20);
			printf("Password: ");
			fgets(sendline, MAXLINE, fp);
			write(sockfd, sendline, 20);
			char udphello[MAXLINE]="hello";
			sendto(udpsockfd, udphello, MAXLINE, 0, pudpaddr, udplen);//let server know your addr
			write_filelist(sockfd);
			flag = 0;sleep(1);//sleep: wait for sockfd thread to get ack
		}
		/*logout*/
		else if(sendline[0]=='O'){
			printf("Logout Done!\n\n");
			flag = 0;/*command done!*/
		}
		/*Delete Member*/
		else if(sendline[0]=='D'&&sendline[1]=='M'){
			printf("Delete member Done!\n\n");
			flag = 0;/*command done!*/
		}
		/*show user*/
		else if(sendline[0]=='S'&&sendline[1]=='U'){
			flag=0;sleep(1);
		}
		/*show file*/
		else if(sendline[0]=='S'&&sendline[1]=='F'){
			flag=0;sleep(1);
		}
		/*request designated client file list*/
		else if(sendline[0]=='R'&&sendline[1]=='F'){
			printf("who: ");
			fgets(sendline, MAXLINE, fp);
			write(sockfd, sendline, 20);

			flag=0;sleep(1);
		}
		else if(sendline[0]=='U'){
			char filename[20]={};
			
			printf("To who: ");
			fgets(sendline, MAXLINE, fp);
			sendline[strlen(sendline)-1]='\0';
			write(sockfd, sendline, 20);
			
			printf("filename: ");
			fgets(filename, 20, fp);
			filename[strlen(filename)-1]='\0';
			write(sockfd, filename, 20);

			flag=0; sleep(1);
		}
		else if(sendline[0]=='D'&&sendline[1]=='O'){
			char filename[20]={};
			
			printf("From who: ");
			fgets(sendline, MAXLINE, fp);
			sendline[strlen(sendline)-1]='\0';
			write(sockfd, sendline, 20);
			
			printf("filename: ");
			fgets(filename, 20, fp);
			filename[strlen(filename)-1]='\0';
			write(sockfd, filename, 20);

			flag=0; sleep(1);
		}
	}
	
	shutdown(sockfd, SHUT_WR);/*EOF on stdin, send FIN*/
	return(NULL);/*let thread terminates*/
}

void* udp_cli(void *arg){
	char recvline[MAXLINE]={};
	int n;
	while(1){
		/*what server write back*/
		recvfrom(udpsockfd, recvline, MAXLINE, 0, NULL, NULL);

		if(strcmp("upload_to_you", recvline)==0){
			char filename[20]={};
			recvfrom(udpsockfd, filename, 20, 0, NULL, NULL);
			int file_fd = open(filename, O_WRONLY | O_CREAT, 0644);

			/*what sender write to you*/
			int get_upload_filesize, rbt;
			recvfrom(udpsockfd, &get_upload_filesize, sizeof(get_upload_filesize), 0, NULL, NULL);
			while(get_upload_filesize>0){
				rbt = recvfrom(udpsockfd, recvline, MAXLINE, 0, NULL, NULL);
				write(file_fd, recvline, rbt);
				printf("writebyte: %d\n", rbt);
				get_upload_filesize-=rbt;
			}
			printf("write OK!\n");
			close(file_fd);
		}
		else if(strcmp("download_from_you", recvline)==0){//designated download from you
			struct sockaddr_in sendaddr;
			char filename[20]={};
			char sendbuf[MAXLINE]={};

			recvfrom(udpsockfd, filename, 20, 0, NULL, NULL);
			printf("filename: %s\n", filename);
			recvfrom(udpsockfd, &sendaddr, sizeof(sendaddr), 0, NULL, NULL);

			char IPdotdec[20]={};
			struct sockaddr_in* in_cliaddr = &sendaddr;//change sockaddr to sockaddr_in, use to get ip address and port number
			printf("****%s:%d \n",inet_ntop(AF_INET, &in_cliaddr->sin_addr, IPdotdec, sizeof(IPdotdec)),(int)in_cliaddr->sin_port);
			

			int download_fd = open(filename , O_RDONLY);
        	struct stat st;
        	fstat(download_fd, &st);
        	int download_filesize = st.st_size;
        	//sleep(3);
        	sendto(udpsockfd, &download_filesize, sizeof(download_filesize), 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
        	printf("sendsize: %d\n", download_filesize);
        	usleep(1000);
        	sendto(udpsockfd, &download_filesize, sizeof(download_filesize), 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
        	
        	printf("size: %d\n", download_filesize);
        	usleep(1000);
        	while(n = readn(download_fd, sendbuf, MAXLINE)){
        		sendto(udpsockfd, sendbuf, n, 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
        		usleep(1000);
        		sendto(udpsockfd, sendbuf, n, 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
        		printf("sendbyte: %d\n", n);
        		memset(sendbuf, 0, sizeof(sendbuf));
        		usleep(1000);
        	}
        	printf("OK!!\n");
        	close(download_fd);
		}
		else if(strcmp("wantsize", recvline)==0){
			char filename[20]={};
			recvfrom(udpsockfd, filename, 20, 0, NULL, NULL);
			int download_fd = open(filename , O_RDONLY);
        	struct stat st;
        	fstat(download_fd, &st);
        	int size = st.st_size;
        	sendto(udpsockfd, &size, sizeof(size), 0, pudpaddr, udplen);
        	close(download_fd);
		}
		else if(strcmp("download_cut", recvline)==0){
			//lseek, get offset and byte, ....
			struct sockaddr_in sendaddr;
			char filename[20]={};
			char sendbuf[MAXLINE]={};

			recvfrom(udpsockfd, filename, 20, 0, NULL, NULL);
			recvfrom(udpsockfd, &sendaddr, sizeof(sendaddr), 0, NULL, NULL);

			int sleeptime;
			recvfrom(udpsockfd, &sleeptime, sizeof(sleeptime), 0, NULL, NULL);
			sleep(sleeptime);//wait for writing in order!!!

			char IPdotdec[20]={};
			struct sockaddr_in* in_cliaddr = &sendaddr;//change sockaddr to sockaddr_in, use to get ip address and port number
			printf("send to...%s:%d \n\n",inet_ntop(AF_INET, &in_cliaddr->sin_addr, IPdotdec, sizeof(IPdotdec)),(int)in_cliaddr->sin_port);

			int download_fd = open(filename ,O_RDONLY);
			int offset, byte;//byte: bytes that need to send to receiver
			recvfrom(udpsockfd, &offset, sizeof(offset), 0, NULL, NULL);
			recvfrom(udpsockfd, &byte, sizeof(byte), 0, NULL, NULL);
			
			sendto(udpsockfd, &byte, sizeof(byte), 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
			usleep(1000);
			sendto(udpsockfd, &byte, sizeof(byte), 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
			usleep(1000);
			
			lseek(download_fd, offset, SEEK_SET);

			while(byte>0){
				n = read(download_fd, sendbuf, MAXLINE);
				if(byte<=n){
					sendto(udpsockfd, sendbuf, byte, 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
					usleep(1000);
			
					sendto(udpsockfd, sendbuf, byte, 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
					usleep(1000);
			
					printf("sendbyte: %d\n", byte);
					byte -= byte;
				}
				else{
					sendto(udpsockfd, sendbuf, n, 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
					usleep(1000);
					sendto(udpsockfd, sendbuf, n, 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
					usleep(1000);
					
					printf("sendbyte: %d\n", n);
					byte -= n;
				}
				memset(sendbuf, 0, sizeof(sendbuf));
			}
			printf("OK!!\n");
        	close(download_fd);
		}
		else if(strcmp("talk hello", recvline)==0){
			recvfrom(udpsockfd, recvline, MAXLINE, 0, NULL, NULL);
			printf("%s\n", recvline);
		}
	}
}
void str_cli(FILE *fp_arg, int sockfd_arg, int udpsockfd_arg, const struct sockaddr* pudpaddr_arg, socklen_t udplen_arg) {
	pthread_t tidA, tidB;
	sockfd = sockfd_arg;
	fp = fp_arg;
	udpsockfd = udpsockfd_arg;
	pudpaddr = pudpaddr_arg;
	udplen = udplen_arg;
	pthread_create(&tidA, NULL, copyto, NULL);
	pthread_create(&tidB, NULL, udp_cli, NULL);

	char recvline[MAXLINE]={};
	int n,x;
	while(1) {//if(FD_ISSET(sockfd, &rset))
		/*what server will write back*/
		read(sockfd, recvline, MAXLINE);

		/*processing always post message*/
		if(strcmp("apm",recvline)==0) {
			read(sockfd, recvline, MAXLINE);
			printf("%s",recvline);
		}
		/*processing registration*/
		if(strcmp("register", recvline)==0) {
			read(sockfd, recvline, MAXLINE);
			printf("%s",recvline);
		}
		/*processing login judge*/
		else if(strcmp("login",recvline)==0) {
			read(sockfd, recvline, MAXLINE);
			printf("%s", recvline);
			//if client wnat to get idindex, write code here and idindex need to be global
			/*command done!*/
        }
        else if(strcmp("talkaddr", recvline)==0){
        	read(sockfd, recvline, MAXLINE);
			printf("%s\n", recvline);
        	if(strcmp("Talk ready!\n", recvline)==0){
	        	struct sockaddr_in eat;
	        	read(sockfd, &eat, sizeof(eat));
	        	ptalkaddr = (struct sockaddr*)&eat;
	        	talklen = sizeof(eat);
	        }
        }
        else if(strcmp("showuser", recvline)==0) {
        	read(sockfd, recvline, MAXLINE);
        	printf("%s\n", recvline);
        }
        else if(strcmp("showfile", recvline)==0) {
        	int per_filename_num;
        	read(sockfd, &per_filename_num, sizeof(per_filename_num));
        	//server
        	read(sockfd, recvline, MAXLINE);
        	printf("%s\n", recvline);
        	//online_client
        	for(x=0;x<per_filename_num;x++){
        		read(sockfd, recvline, MAXLINE);
        		printf("%s\n", recvline);
        	}
        }
        else if(strcmp("request_file_list", recvline)==0) {
        	read(sockfd, recvline, MAXLINE);
        	printf("%s\n", recvline);
        }
        else if(strcmp("uploadack", recvline)==0){
        	read(sockfd, recvline, MAXLINE);
        	printf("%s\n", recvline);
        	if(strcmp("Upload ready!\n", recvline)==0){
        		struct sockaddr_in sendaddr;
        		char filename[20]={};
        		char sendbuf[MAXLINE]={};
        		
        		read(sockfd, filename, 20);
        		read(sockfd, &sendaddr, sizeof(sendaddr));
        		
        		int upload_fd = open(filename, O_RDONLY);
        		struct stat st;
        		fstat(upload_fd, &st);
        		int upload_filesize = st.st_size;
        		sendto(udpsockfd, &upload_filesize, sizeof(upload_filesize), 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
        		usleep(1000);
        		while(n = readn(upload_fd, sendbuf, MAXLINE)){
        			sendto(udpsockfd, sendbuf, n, 0, (struct sockaddr*)&sendaddr, sizeof(sendaddr));
        			printf("sendbyte: %d\n", n);
        			memset(sendbuf, 0, sizeof(sendbuf));
        			usleep(1000);
        		}
        		printf("OK!!\n");
        		close(upload_fd);
        	}
        }
        else if(strcmp("downloadack", recvline)==0){
        	read(sockfd, recvline, MAXLINE);//read returnstring
        	printf("%s\n", recvline);
        	if(strcmp("Download ready!\n", recvline)==0){
	        	char filename[20]={};
				read(sockfd, filename, 20);
				printf("filename: %s___\n",filename);
				
				/*what sender write to you*/
				int get_download_filesize, rbt;
				printf("haven't get size: %d\n", get_download_filesize);
				
				struct sockaddr_in testaddr;
				socklen_t len = sizeof(testaddr);

				recvfrom(udpsockfd, &get_download_filesize, sizeof(get_download_filesize), 0, (struct sockaddr*)&testaddr, &len);
				//get_download_filesize = 10377;

				int file_fd = open(filename, O_WRONLY | O_CREAT, 0644);

				/*				
				char IPdotdec[20]={};
				struct sockaddr_in* in_cliaddr = &testaddr;//change sockaddr to sockaddr_in, use to get ip address and port number
				printf("get size from......%s:%d \n",inet_ntop(AF_INET, &in_cliaddr->sin_addr, IPdotdec, sizeof(IPdotdec)),(int)in_cliaddr->sin_port);
				*/

				printf("size: %d\n", get_download_filesize);
				while(get_download_filesize>0){
					rbt = recvfrom(udpsockfd, recvline, MAXLINE, 0, NULL, NULL);
					write(file_fd, recvline, rbt);
					printf("writebyte: %d\n", rbt);
					get_download_filesize-=rbt;
					printf("size: %d\n", get_download_filesize);
					memset(recvline, 0, sizeof(recvline));
				}
				printf("write OK!\n");
				close(file_fd);
			}
			else if(strcmp("Download from server ready!\n", recvline)==0){
				char filename[20]={};
				read(sockfd, filename, 20);
				int file_fd = open(filename, O_WRONLY | O_CREAT, 0644);

				int size, rbt;
				read(sockfd, &size, sizeof(size));
				while(size>0){
					memset(recvline, 0, sizeof(recvline));
					rbt = read(sockfd, recvline, MAXLINE);
					write(file_fd, recvline, rbt);
					printf("writebyte: %d\n", rbt);
					size -= rbt;
				}
				printf("write OK!\n");
				close(file_fd);
			}
			else if(strcmp("Download cut ready!\n", recvline)==0){
				struct sockaddr_in comefromaddr;
				socklen_t len = sizeof(comefromaddr);
				char filename[20]={};
				int havenum;
				read(sockfd, filename, 20);
				read(sockfd, &havenum, sizeof(havenum));
				int file_fd = open(filename, O_WRONLY | O_CREAT, 0644);

				int byte, rbt;
				for(x=0; x<havenum; x++){
					recvfrom(udpsockfd, &byte, sizeof(byte), 0, (struct sockaddr*)&comefromaddr, &len);
					
					char IPdotdec[20]={};
					struct sockaddr_in* in_cliaddr = &comefromaddr;//change sockaddr to sockaddr_in, use to get ip address and port number
					printf("get from %s:%d \n",inet_ntop(AF_INET, &in_cliaddr->sin_addr, IPdotdec, sizeof(IPdotdec)),(int)in_cliaddr->sin_port);
					
					while(byte>0){
						rbt = recvfrom(udpsockfd, recvline, MAXLINE, 0, NULL, NULL);
						write(file_fd, recvline, rbt);
						printf("writebyte: %d\n", rbt);
						byte-=rbt;
					}
				}
				printf("write OK!\n");
				close(file_fd);
			}
        }
	}
}

int main (int argc, char **argv) {
	int sockfd, n;
	int udpsockfd;
	char recvline[MAXLINE+1]; /*MAXLINE=4096: max text line length */ 
	struct sockaddr_in servaddr, udpaddr; /*declare server address*/

	//we initialize servddr at global!!!
	if(argc!=3) /*deal with error message*/ 
		printf("usage: a.out <IPaddress>");
	
	//TCP: server is at 7777
	if((sockfd=socket(AF_INET, SOCK_STREAM,0)) < 0)
		printf("socket error");
	fill_sockaddr_client(&servaddr, argv[1], AF_INET, atoi(argv[2]));
	connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

	//UDP: server is at 7772
	if((udpsockfd=socket(AF_INET, SOCK_DGRAM, 0)) <0)
		printf("socket error");
	fill_sockaddr_client(&udpaddr, argv[1], AF_INET, atoi(argv[2])-5);
	
	str_cli(stdin, sockfd, udpsockfd, (struct sockaddr *)&udpaddr, sizeof(udpaddr));
	
	exit(0);
}
