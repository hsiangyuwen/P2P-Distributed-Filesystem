#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include "netio.h"
#define MAXLINE 2048
#define LISTENQ 1024
#define CLIENT_NUM 1000
int udpsockfd;
struct sockaddr_in IP_Port_list[CLIENT_NUM+1];//index: idindex, use this to send file
char returnstring[MAXLINE]={};//tell client what to do string
char filelist[CLIENT_NUM+1][50][20]={};//store client have what file

struct client {
	int islogin;
	int filenum;
	char ID[20];
	char password[20];
	//int connfd;//know who is using this client because every thread know client sockfd
};
struct client client_info[CLIENT_NUM+1];/*accept 1000 clients and stort from index 1 to 1000*/
int cur_clinum = 0;

void lookidlist(){
	int idlist_fd = open("idlist",O_RDONLY | O_CREAT, 0644);
	int x;
	for(x=1; x<=cur_clinum; x++) {
		if(readstring(idlist_fd, client_info[x].ID, 20)==0)
			break;
		readstring(idlist_fd, client_info[x].password, 20);
		if(strcmp(client_info[x].ID, "empty"))
			printf("%s - port:%d\n", client_info[x].ID, IP_Port_list[x].sin_port);
	}
	close(idlist_fd);
}

void writeidlist(char *buff){
	int idlist_fd = open("idlist",O_WRONLY | O_APPEND);
	writen(idlist_fd, buff, strlen(buff));
	close(idlist_fd);
}

void write_returnstring(int sockfd, char *buff){
	memset(returnstring, 0, MAXLINE);
	strcat(returnstring, buff);
	write(sockfd, returnstring, MAXLINE);
}
void sendto_returnstring(int udpsockfd, char *buff, struct sockaddr*pcliaddr, socklen_t clilen){
	memset(returnstring, 0, MAXLINE);
	strcat(returnstring, buff);
	sendto(udpsockfd, returnstring, MAXLINE, 0, pcliaddr, clilen);
}

void str_echo(int sockfd) {
	char buff[MAXLINE];//eat cilent write string
	int n,x,y;
	int idindex;//know "client and this thread" what which ID you are using

	while((n = read(sockfd, buff, MAXLINE))>0) {//if(FD_ISSET(sockfd, &rset))
		lookidlist();
		
		/*eaten command is in buff*/
		/*processing welcome message and refresh filelist*/
		if(strcmp(buff, "wantapm")==0) {
			/*refresh filelist*/
			int filenum;
			read(sockfd, &filenum, sizeof(filenum));
			client_info[idindex].filenum = filenum;
			for(x=0; x<filenum; x++)
				read(sockfd, filelist[idindex][x], 20);
			/*processing welcome message*/
			write_returnstring(sockfd, "apm");
			if(idindex==0) {//client is not login
				memset(buff, 0, sizeof(buff));
				strcat(buff, "*************Welcome*****************\n[R]egister\t[L]ogin\n");
				write(sockfd, buff, MAXLINE);
			}
			else {//client is login
				memset(buff, 0, sizeof(buff));
				strcat(buff, "*************hello ");
				strcat(buff, client_info[idindex].ID);
				strcat(buff, "*****************\n[SU]Show User\t[T]ell\t[DM]Delete Member\t[O]Logout\n[SF]Show File\t[RF]Request file list\t[DO]wnload\t[U]pload\n");
				write(sockfd, buff, MAXLINE);
			}
		}
		/*processing registration*/
		else if(buff[0]=='R'&&buff[1]!='F'){
			char ID[20]={};//check ID has been used or not
			char idp[40]={};//to write idlist

			read(sockfd, buff, 20);//ID
			strcat(ID, buff);
			ID[strlen(ID)-1]='\0';
			buff[strlen(buff)-1]=' ';buff[strlen(buff)]='\0';
			strcat(idp, buff);
			read(sockfd, buff, 20);//password
			//buff[strlen(buff)]='\0';
			strcat(idp, buff);
			//idp[strlen(idp)]='\0';
			for(x=1; x<=cur_clinum; x++) {
				if(strcmp(client_info[x].ID, ID)==0){
					write_returnstring(sockfd, "register");
					write_returnstring(sockfd, "The ID has been used!\n");
					break;
				}
			}
			if(x==(cur_clinum+1)) {//hasn't been used
				writeidlist(idp);
				write_returnstring(sockfd, "register");
				write_returnstring(sockfd, "Registration Done!\n\n");
				cur_clinum++;
			}
		}
		/*processing login*/
		else if(buff[0]=='L'){
			char ID[20]={}, password[20]={};
			read(sockfd, ID, 20);ID[strlen(ID)-1]='\0';
			read(sockfd, password, 20);password[strlen(password)-1]='\0';

			write_returnstring(sockfd, "login");
			for(x=1; x<=cur_clinum; x++) {
				if(strcmp(client_info[x].ID, ID)==0){
					if(strcmp(client_info[x].password, password)==0){
						write_returnstring(sockfd, "LOGIN OK!!\n\n");
						idindex=x;//every thread has use different str_echo
						//**if client need idindex, write code at here!
						client_info[x].islogin = 1;
					}
					else{
						write_returnstring(sockfd, "Password incorrect!\n\n");
						break;
					}
				}
			}
			if(x==(cur_clinum+1))//didn't have this member
				write_returnstring(sockfd, "No this member\n\n");
			
			/*get client's udp IP_Port*/
			socklen_t len = sizeof(IP_Port_list[idindex]);
			recvfrom(udpsockfd, buff, MAXLINE, 0, (struct sockaddr *)&IP_Port_list[idindex], &len);
			/*refresh filelist*/
			int filenum;
			read(sockfd, &filenum, sizeof(filenum));
			client_info[idindex].filenum = filenum;
			for(x=0; x<filenum; x++)
				read(sockfd, filelist[idindex][x], 20);
			/*print login member's filelist*/
			if(idindex!=0){
				printf("%s's filelist:\n", client_info[idindex].ID);
				for(x=0; x<client_info[idindex].filenum; x++)
					printf("%s\n", filelist[idindex][x]);
				putchar('\n');
			}
		}
		/*processing logout*/
		else if(buff[0]=='O'){
			client_info[idindex].islogin=0;
			//client_info[idindex].connfd=0;
			memset(&IP_Port_list[idindex], 0, sizeof(IP_Port_list[idindex]));
			idindex = 0;
		}
		/*processing delete member*/
		else if(buff[0]=='D'&&buff[1]=='M'){
			memset(&client_info[idindex], 0, sizeof(client_info[idindex]));
			memset(&IP_Port_list[idindex], 0, sizeof(IP_Port_list[idindex]));
			remove("idlist");
			int idlist_fd = open("idlist", O_WRONLY | O_CREAT, 0644);
			char idp[50]={};
			for(x=1; x<=cur_clinum; x++){
				strcat(idp, client_info[x].ID);
				strcat(idp, " ");
				strcat(idp, client_info[x].password);
				idp[strlen(idp)]='\n';idp[strlen(idp)+1]='\0';
				if(x==idindex)
					write(idlist_fd, "empty empty\n", strlen("empty empty\n"));
				else
					write(idlist_fd, idp, strlen(idp));
				memset(idp, 0, sizeof(idp));
			}
			idindex = 0;
		}

		/*processing show user*/
		else if(buff[0]=='S'&&buff[1]=='U'){
			char loginlist[MAXLINE]={};
			struct sockaddr_in* online_cliaddr;
			char IPbuff[20]={}, portbuff[20]={};
			for(x=1;x<=cur_clinum;x++){
				if(client_info[x].islogin){
					strcat(loginlist, client_info[x].ID);
					online_cliaddr = &(IP_Port_list[x]);
					strcat(loginlist, " IP:");
					strcat(loginlist, inet_ntop(AF_INET, &online_cliaddr->sin_addr, IPbuff, sizeof(IPbuff)));
					strcat(loginlist, " Port:");
					sprintf(portbuff, "%d", (int)online_cliaddr->sin_port);
					strcat(loginlist, portbuff);
					strcat(loginlist, "\n");
				}
			}
			write_returnstring(sockfd, "showuser");
			write(sockfd, loginlist, MAXLINE);
		}
		/*processing show files*/
		else if(buff[0]=='S'&&buff[1]=='F'){
			char per_fileline[MAXLINE]={};
			int per_fileline_num=0;
			for(x=1; x<=cur_clinum; x++){
				if(client_info[x].islogin)
					per_fileline_num++;
			}
			write_returnstring(sockfd, "showfile");
			write(sockfd, &per_fileline_num, sizeof(per_fileline_num));
			
			//server's file
			DIR *d;
			struct dirent *dir;
			d = opendir(".");
			strcat(per_fileline, "Server\n");
			while( (dir = readdir(d)) != NULL ){
				if(dir->d_name[0]!='.'&&dir->d_name[strlen(dir->d_name)-1]!='~'&&dir->d_name[strlen(dir->d_name)-1]!='\n'){
					strcat(per_fileline, dir->d_name);
					strcat(per_fileline, "\t");
				}
			}
			strcat(per_fileline, "\n");
			write(sockfd, per_fileline, MAXLINE);
			//online_client's file
			for(x=1; x<=cur_clinum; x++){
				if(client_info[x].islogin){
					memset(per_fileline, 0, sizeof(per_fileline));
					strcat(per_fileline, client_info[x].ID);
					strcat(per_fileline, "\n");
					for(y=0;y<client_info[x].filenum;y++){
						strcat(per_fileline, filelist[x][y]);
						strcat(per_fileline, "\t");
					}
					strcat(per_fileline, "\n");
					write(sockfd, per_fileline, MAXLINE);
				}
			}
		}
		/*processing request designated client fileline*/
		else if(buff[0]=='R'&&buff[1]=='F'){
			char per_fileline[MAXLINE]={};
			char ID[20]={};
			read(sockfd, ID, 20);//request whose
			ID[strlen(ID)-1]='\0';
			write_returnstring(sockfd, "request_file_list");
			for(x=1;x<=cur_clinum; x++){
				if(strcmp(ID, client_info[x].ID)==0){//have this member
					if(client_info[x].islogin){
						strcat(per_fileline, client_info[x].ID);
						strcat(per_fileline, "\n");
						for(y=0;y<client_info[x].filenum;y++){
							strcat(per_fileline, filelist[x][y]);
							strcat(per_fileline, "\t");
						}
						strcat(per_fileline, "\n");
						write(sockfd, per_fileline, MAXLINE);
						break;
					}
					else
						write_returnstring(sockfd, "The member is not online!\n");
				}
			}
			if(x==(cur_clinum+1)){//no this member
				write_returnstring(sockfd, "No this member!\n");
			}	
		}
		/*processing telling*/
		else if(buff[0]=='T'){
			char ID[20]={};
			read(sockfd, ID, 20);
			write_returnstring(sockfd, "talkaddr");

			for(x=1;x<=cur_clinum; x++){
				if(strcmp(ID, client_info[x].ID)==0){//have this member
					if(client_info[x].islogin){
						write_returnstring(sockfd, "Talk ready!\n");
						write(sockfd, &IP_Port_list[x], sizeof(IP_Port_list[x]));
						break;
					}
					else
						write_returnstring(sockfd, "The member is not online!\n");
				}
			}
			if(x==(cur_clinum+1))//no this member
				write_returnstring(sockfd, "No this member!\n");
		}

		/*processing upload file to designated client*/
		else if(buff[0]=='U'){
			char filename[20]={};
			char ID[20]={};
			read(sockfd, ID, 20);//request whose
			read(sockfd, filename, 20);//tell two client what filename you need to open

			write_returnstring(sockfd, "uploadack");
			for(x=1;x<=cur_clinum; x++){
				if(strcmp(ID, client_info[x].ID)==0){//have this member
					if(client_info[x].islogin){
						/*use UDP to tell receiver to open file_fd and get file*/
						sendto_returnstring(udpsockfd, "upload_to_you", (struct sockaddr*)&IP_Port_list[x], sizeof(IP_Port_list[x]));
						sendto(udpsockfd, filename, 20, 0, (struct sockaddr*)&IP_Port_list[x], sizeof(IP_Port_list[x]));
						/*use TCP to tell sender's sockfd "1.the filename" and "2.receiver's UDP IP_Port"*/
						write_returnstring(sockfd, "Upload ready!\n");
						write(sockfd, filename, 20);
						write(sockfd, &IP_Port_list[x], sizeof(IP_Port_list[x]));
						break;
					}
					else
						write_returnstring(sockfd, "The member is not online!\n");
				}
			}
			if(x==(cur_clinum+1))//no this member
				write_returnstring(sockfd, "No this member!\n");
		}

		/*processing download file from designated client or server*/
		else if(buff[0]=='D'&&buff[1]=='O'){
			char filename[20]={};
			char ID[20]={};
			read(sockfd, ID, 20);//request whose
			read(sockfd, filename, 20);//tell two client what filename you need to open

			write_returnstring(sockfd, "downloadack");
			
			//#1: designated client
			for(x=1;x<=cur_clinum; x++){
				if(strcmp(ID, client_info[x].ID)==0){//have this member
					if(client_info[x].islogin){
						/*use TCP to tell sender's sockfd "1.the filename" and "2.receiver's UDP IP_Port"*/
						write_returnstring(sockfd, "Download ready!\n");
						write(sockfd, filename, 20);
						/*use UDP to tell receiver to open file_fd and send file*/
						sendto_returnstring(udpsockfd, "download_from_you", (struct sockaddr*)&IP_Port_list[x], sizeof(IP_Port_list[x]));
						sendto(udpsockfd, filename, 20, 0, (struct sockaddr*)&IP_Port_list[x], sizeof(IP_Port_list[x]));
						sendto(udpsockfd, &IP_Port_list[idindex], sizeof(IP_Port_list[idindex]), 0, (struct sockaddr*)&IP_Port_list[x], sizeof(IP_Port_list[x]));
						
						break;
					}
					else
						write_returnstring(sockfd, "The member is not online!\n");
				}
			}

			if(x==(cur_clinum+1) && strcmp(ID, "server")!=0)//no this member
				write_returnstring(sockfd, "No this member!\n");
			else if(strcmp(ID, "server")==0){
				int havenum = 0;
				int havelist[CLIENT_NUM+1]={0};
				for(x=1; x<=cur_clinum; x++){
					if(client_info[x].islogin && x!=idindex){//need to be online
						for(y=0; y<client_info[x].filenum; y++){
							if(strcmp(filelist[x][y], filename)==0){//have this file
								havelist[havenum]=x;
								havenum++;
								break;
							}
						}
					}
				}
				//#2: just server have
				if(havenum==0) {
					write_returnstring(sockfd, "Download from server ready!\n");
					write(sockfd, filename, 20);

					char sendbuf[MAXLINE]={};
					int download_fd = open(filename, O_RDONLY);
					struct stat st;
					fstat(download_fd, &st);
					int size = st.st_size;
					write(sockfd, &size, sizeof(size));
					while(n = readn(download_fd, sendbuf, MAXLINE)){
						write(sockfd, sendbuf, n);
						printf("sendbyte: %d\n", n);
						memset(sendbuf, 0, sizeof(sendbuf));
					}
					printf("OK!!\n");
					close(download_fd);
				}
				//#3: need to cut!!!
				else{
					sendto_returnstring(udpsockfd, "wantsize", (struct sockaddr*)&IP_Port_list[havelist[0]], sizeof(IP_Port_list[havelist[0]]));
					sendto(udpsockfd, filename, 20, 0, (struct sockaddr*)&IP_Port_list[havelist[0]], sizeof(IP_Port_list[havelist[0]]));
					
					int size;
					recvfrom(udpsockfd, &size, sizeof(size), 0, NULL, NULL);

					write_returnstring(sockfd, "Download cut ready!\n");
					write(sockfd, filename, 20);
					write(sockfd, &havenum, sizeof(havenum));

					int cutsize = size/havenum;
					int offset, byte;
					//0 ~ havenum-2
					for(x=0; x<havenum-1; x++){
						sendto_returnstring(udpsockfd, "download_cut", (struct sockaddr*)&IP_Port_list[havelist[x]], sizeof(IP_Port_list[havelist[x]]));
						offset = x*cutsize;
						byte = cutsize;
						sendto(udpsockfd, filename, 20, 0, (struct sockaddr*)&IP_Port_list[havelist[x]], sizeof(IP_Port_list[havelist[x]]));
						sendto(udpsockfd, &IP_Port_list[idindex], sizeof(IP_Port_list[idindex]), 0, (struct sockaddr*)&IP_Port_list[havelist[x]], sizeof(IP_Port_list[havelist[x]]));
						int sleeptime = 2*x;
						sendto(udpsockfd, &sleeptime, sizeof(sleeptime), 0, (struct sockaddr*)&IP_Port_list[havelist[x]], sizeof(IP_Port_list[havelist[x]]));
						sendto(udpsockfd, &offset, sizeof(offset), 0, (struct sockaddr*)&IP_Port_list[havelist[x]], sizeof(IP_Port_list[havelist[x]]));
						sendto(udpsockfd, &byte, sizeof(byte), 0, (struct sockaddr*)&IP_Port_list[havelist[x]], sizeof(IP_Port_list[havelist[x]]));
					}
					//havenum-1
						sendto_returnstring(udpsockfd, "download_cut", (struct sockaddr*)&IP_Port_list[havelist[havenum-1]], sizeof(IP_Port_list[havelist[havenum-1]]));
						offset = (havenum-1)*cutsize;
						byte = size - offset;//remain bytes
						sendto(udpsockfd, filename, 20, 0, (struct sockaddr*)&IP_Port_list[havelist[havenum-1]], sizeof(IP_Port_list[havelist[havenum-1]]));
						sendto(udpsockfd, &IP_Port_list[idindex], sizeof(IP_Port_list[idindex]), 0, (struct sockaddr*)&IP_Port_list[havelist[havenum-1]], sizeof(IP_Port_list[havelist[havenum-1]]));
						int sleeptime = havenum-1;
						sendto(udpsockfd, &sleeptime, sizeof(sleeptime), 0, (struct sockaddr*)&IP_Port_list[havelist[havenum-1]], sizeof(IP_Port_list[havelist[havenum-1]]));
						sendto(udpsockfd, &offset, sizeof(offset), 0, (struct sockaddr*)&IP_Port_list[havelist[havenum-1]], sizeof(IP_Port_list[havelist[havenum-1]]));
						sendto(udpsockfd, &byte, sizeof(byte), 0, (struct sockaddr*)&IP_Port_list[havelist[havenum-1]], sizeof(IP_Port_list[havelist[havenum-1]]));
				}
			}
		}
	}
}


static void * doit(void *arg){
	int connfd;
	connfd = *((int *) arg);
	free(arg);

	pthread_detach(pthread_self());

	str_echo(connfd);/*same function as before*/
	close(connfd);/*done with connected socket*/

	return(NULL);
}

int main(int argc, char **argv) {
	int listenfd, *iptr;
	pthread_t tid;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t addrlen = sizeof(cliaddr);
	
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	fill_sockaddr_server(&servaddr, INADDR_ANY, AF_INET, atoi(argv[1]));
	bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	listen(listenfd, LISTENQ); /*LISTENQ=1024, it is a backlog*/

	struct sockaddr_in udpaddr;
	//udpsockfd initial is at global
	udpsockfd = socket(AF_INET, SOCK_DGRAM, 0);
	fill_sockaddr_server(&udpaddr, INADDR_ANY, AF_INET, atoi(argv[1])-5);
	bind(udpsockfd, (struct sockaddr *)&udpaddr, sizeof(udpaddr));
	

	//build up client_info when starting server
	int x;
	for(x=0;x<=CLIENT_NUM;x++){
		client_info[x].islogin = 0;
		memset(client_info[x].ID,0,sizeof(client_info[x].ID));
		memset(client_info[x].password,0,sizeof(client_info[x].password));
		//client_info[x].connfd = 0;
	}
	int idlist_fd = open("idlist",O_RDONLY | O_CREAT, 0644);
	for( ; cur_clinum<=CLIENT_NUM; cur_clinum++) {
		if(readstring(idlist_fd, client_info[cur_clinum+1].ID, 20)==0)//
			break;
		readstring(idlist_fd, client_info[cur_clinum+1].password, 20);
	}
	close(idlist_fd);

	for (;;) {
		iptr = malloc(sizeof(int));
		*iptr = accept(listenfd, (struct sockaddr *) &cliaddr, (socklen_t *) &addrlen);
		//IP_Port_list[*iptr] = cliaddr;
		//char IPdotdec[20];//use inet_ntop to change client IP address to xxx.xxx.xxx.xxx
		//inet_ntop(AF_INET, (struct in_addr *) &cliaddr.sin_addr.s_addr, IPdotdec, sizeof(IPdotdec));
		//printf("connection from %s, port: %d\n", IPdotdec, cliaddr.sin_port);
		
		pthread_create(&tid, NULL, &doit, iptr);
	}
}

