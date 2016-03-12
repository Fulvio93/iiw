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

#define BUFFLEN 512 //Max length of buffer
#define PORT 8888   //The port on which to listen for incoming data

char path[256];
struct sockaddr_in sock_serv, clnt;
int sfd;
ssize_t recv_len;
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
void list_elements()
{
	dp = opendir (path);

	while ((ep = readdir(dp)))
	{
		sendbuf(ep->d_name);
	}

	(void) closedir (dp);
}
void get_data(char* buf) {
	int fd;
	char file_request[BUFFLEN] = "";
	char buftemp[BUFFLEN];
	off_t count = 0, m, sz;//long
	long int n;
	int l = sizeof(struct sockaddr_in);
	struct stat buffer;

	strcat(file_request, path);
	strcat(file_request, buf);

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
void parse_data(char* buf)
{
	int swc;
	ssize_t bytesread;
	char getbuf[BUFFLEN];
	if(strncmp(buf,"list",4) == 0)
		swc=0;
	if(strncmp(buf,"get",3) == 0)
		swc=1;
	if(strncmp(buf,"put",3) == 0)
		swc=2;
	switch (swc){
		case 0:{
			puts("LIST request");
			list_elements(buf);
			unlock_client();
			puts("Done");
			break;
		};
		case 1:{
			puts("GET request");
			puts("Waiting for file name... ");

			memset(getbuf,0,BUFFLEN);
			bytesread = recvfrom(sfd, getbuf, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen);

			if (bytesread == -1)
			{
				error("recvfrom()");
			}
			get_data(getbuf);
			break;
		};
		case 2:{
			puts("PUT request");
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

	//keep listening for data
	while(1)
	{
		printf("Waiting for data...");
		fflush(stdout);

		//try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(sfd, buf, BUFFLEN, 0, (struct sockaddr *) &clnt, &slen)) == -1)
		{
			error("recvfrom()");
		}

		//print details of the client and the data received
		printf("Received packet from: %s, port: %d\n", inet_ntoa(clnt.sin_addr), ntohs(clnt.sin_port));

		parse_data(buf);

	}

}
