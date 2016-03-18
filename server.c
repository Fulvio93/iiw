#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<dirent.h>
#include<unistd.h>
#include<errno.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<fcntl.h>
#include<sys/uio.h>
#include<sys/stat.h>
#include <sys/wait.h>

#define BUFFLEN 512 //Max length of buffer
#define PORT 8888   //The port on which to listen for incoming data

char path[256];
struct sockaddr_in sock_serv, clnt;
int sfd;
socklen_t slen = sizeof(clnt);
DIR *dp;
struct dirent *ep;


void error(char *str)
{
	perror(str);
	exit(1);
}
void sendbuf(char* buf)
{
	if (sendto(sfd, buf, BUFFLEN, 0, (struct sockaddr*) &clnt, slen) == -1)
	{
		error("sendto()");
	}
}
void unlock_client()
{
	if (sendto(sfd, "", 0, 0, (struct sockaddr*) &clnt, slen) == -1)
	{
		error("sendto()");
	}
}
void list_elements_to_client()
{
	dp = opendir (path);

	while ((ep = readdir(dp)))
	{
		sendbuf(ep->d_name);
	}

	(void) closedir (dp);
}
void send_data_to_client()
{
	int fd;
	char file_request[BUFFLEN] = "";
	char buftemp[BUFFLEN];
	off_t count = 0, m, sz;//long
	long int n;
	int l = sizeof(struct sockaddr_in);
	struct stat buffer;
	ssize_t bytesread;
	char getbuf[BUFFLEN];

	puts("GET request");
	puts("Waiting for file name... ");

	memset(getbuf,0,BUFFLEN);
	bytesread = recvfrom(sfd, getbuf, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen);

	if (bytesread == -1)
	{
		error("recvfrom()");
	}

	strcat(file_request, path);
	strcat(file_request, getbuf);

	printf("file richiesto: %s\n", file_request);

	fd = open(file_request, O_RDONLY);
	if (fd != -1) {

		//dimensione del file
		if (stat(file_request, &buffer) == -1)
			error("stat fail");
		else
			sz = buffer.st_size;

		memset(&buftemp, 0, BUFFLEN);

		n = read(fd, buftemp, BUFFLEN);
		while (n) {
			if (n == -1) {
				perror("read file");
				break;
			}


			m = sendto(sfd, buftemp, n, 0, (struct sockaddr *) &sock_serv, l);

			if (m == -1)
				error("send error");
			count += m;
			memset(buftemp, 0, BUFFLEN);
			n = read(fd, buftemp, BUFFLEN);
		}


		//sblocco server
		m = sendto(sfd, buftemp, 0, 0, (struct sockaddr *) &sock_serv, l);

		printf("Numero byte trasferiti : %zd\n", count);
		printf("Su un numero totale di : %zd \n", sz);

		close(fd);
	}
	else{
		perror("open file fail");
		unlock_client();
	}
}
void receive_data_from_client()
{
	char file_to_receive[BUFFLEN];
	char path_file_in_server[BUFFLEN]="";
	ssize_t bytesread,byteswritten,count;
	int fd;
	bytesread = recvfrom(sfd, file_to_receive, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen); //qui ricevo il nome del file
	if (bytesread == -1)
	{
		error("recvfrom()");
	}
	printf("filename ricevuto: %s\n",file_to_receive);
	strcat(path_file_in_server,path);
	strcat(path_file_in_server,file_to_receive);
	fd = open(path_file_in_server,O_CREAT|O_WRONLY|O_TRUNC,0600);
	if((fd==-1)){
		error("open fail");
	}
	count = 0;
	memset(file_to_receive,0, BUFFLEN);
	bytesread = recvfrom(sfd,file_to_receive, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen);//qui inizio a ricevere i byte

	while(bytesread)
	{
		if (bytesread == -1)
		{
			error("recvfrom()");
		}
		count += bytesread;
		byteswritten=write(fd,file_to_receive,bytesread);
		if(byteswritten==-1)
			error("write file fail");

		memset(file_to_receive,0,BUFFLEN);
		bytesread = recvfrom(sfd, file_to_receive, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen);
	}
	printf("Numero bytes ricevuti : %zd\n",count);
	close(fd);
}
void parse_data(char* buf)
{
	int swc=-1;

	if(strncmp(buf,"list",4) == 0)
		swc=0;
	if(strncmp(buf,"get",3) == 0)
		swc=1;
	if(strncmp(buf,"put",3) == 0)
		swc=2;
	memset(buf,0,BUFFLEN);
	switch (swc){
		case 0:{
			puts("LIST request");
			list_elements_to_client();
			unlock_client();
			puts("Done");
			break;
		};
		case 1:{
			send_data_to_client();
			break;
		};
		case 2:{
			puts("PUT request");
			receive_data_from_client();
			break;
		};
		default:{
			puts("not valid command");
			unlock_client();
		};
	}
}
int check_path()
{
	dp = opendir (path);
	if (dp == NULL)
	{
		puts("Couldn't open the directory");
		return 0;
	}
	else
	{
		puts("Path correct");
		(void) closedir (dp);
		return 1;
	}
}
int main(void)
{
	int exitwhile = 0;
	char buf[BUFFLEN];
	pid_t pid;

	ssize_t recv_len;
	while(exitwhile == 0)
	{
		printf("Insert folder path (with final slash) : ");
		if(scanf("%s", path)==-1)
		{
			error("scanf");
		}
		if(check_path())
			exitwhile=1;
	}
	//create a UDP socket
	if ((sfd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		error("socket");
	}

	// zero out the structure
	memset((char *) &sock_serv, 0, sizeof(sock_serv));

	sock_serv.sin_family = AF_INET;
	sock_serv.sin_port = htons(PORT);
	sock_serv.sin_addr.s_addr = htonl(INADDR_ANY);

	//bind socket to port
	if( bind(sfd , (struct sockaddr*)&sock_serv, sizeof(sock_serv) ) == -1)
	{
		error("bind");
	}

	//ascolto le richieste del client
	while(1)
	{
		printf("Waiting for data...\n");
		fflush(stdout);

		//try to receive some data, this is a blocking call
		recv_len = recvfrom(sfd, buf, BUFFLEN, 0, (struct sockaddr *) &clnt, &slen);
		if (recv_len == -1)
		{
			error("recvfrom()");
		}

		if((pid=fork())==-1)
		{
			error("fork fail");
		}
		if(pid == 0) {
			printf("I'm the child\n");

			//print details of the client and the data received
			printf("Received packet from: %s, port: %d\n", inet_ntoa(clnt.sin_addr), ntohs(clnt.sin_port));

			parse_data(buf);

			exit(0);
		}
		sleep(1); //faccio partire prima il figlio
	}
}
