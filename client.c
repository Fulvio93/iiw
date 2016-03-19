#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/stat.h>
#include <sys/wait.h>

#define SERVER "127.0.0.1"
#define BUFFLEN 512 //Max length of buffer
#define PORT 8888   //The port on which to send data

struct sockaddr_in sock_serv;
int sfd;
socklen_t slen=sizeof(sock_serv);

void error(char *s)
{
	perror(s);
	exit(1);
}
void unlock_server()
{
	if (sendto(sfd, " ", 0, 0, (struct sockaddr*) &sock_serv, slen) == -1)
	{
		error("sendto()");
	}
}
void get_data_from_server()
{
	printf("inizio operazione get -- porta server:%d\n",sock_serv.sin_port);
	int fd;
	char buf[BUFFLEN];
	char filename_to_get[BUFFLEN];
	ssize_t bytesread,byteswritten, count = 0, conta = 0;


	fprintf(stdout,"Write the name of the file you want download: ");
	if(scanf("%s", filename_to_get)==-1)
		error("scanf");
	if (sendto(sfd, filename_to_get, strlen(filename_to_get) , 0 , (struct sockaddr *) &sock_serv, slen)==-1)
	{
		error("sendto()");
	}
	fd = open(filename_to_get,O_CREAT|O_WRONLY|O_TRUNC,0600);

	if((fd==-1)){
		error("open fail");
	}
	count = 0;
	memset(buf,0, BUFFLEN);
	bytesread = recvfrom(sfd, buf, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen);

	while(bytesread)
	{
		if (bytesread == -1)
		{
			error("recvfrom()");
		}
		count += bytesread;
		byteswritten=write(fd,buf,bytesread);
		conta += byteswritten;
		if(byteswritten==-1)
			error("write file fail");

		memset(buf,0,BUFFLEN);
		bytesread = recvfrom(sfd, buf, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen);
	}
	printf("Numero byte ricevuti : %zd\n",count);
	printf("Numero byte scritti : %zd\n",conta);
	close(fd);

}
void list_from_server()
{	printf("inizio operazione list -- porta server:%d\n",sock_serv.sin_port);
	char buf[BUFFLEN];
	ssize_t bytesread, count = 0;
	memset(buf,0, BUFFLEN);
	bytesread = recvfrom(sfd, buf, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen);

	while(bytesread)
	{
		if (bytesread == -1)
		{
			error("recvfrom()");
		}
		count += bytesread;
		puts(buf);
		bytesread = recvfrom(sfd, buf, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen);
	}
}
void put_file_to_server()
{
	printf("inizio operazione put -- porta server:%d\n",sock_serv.sin_port);
	char filename_to_put[BUFFLEN];
	int fd;
	struct stat buffer;
	off_t count = 0, m, sz;//long
	long int n;
	int l = sizeof(struct sockaddr_in);
	char buftemp[BUFFLEN];

	fprintf(stdout,"Write the name of the file you want upload: ");
	if(scanf("%s", filename_to_put)==-1)
		error("scanf");
	if (sendto(sfd, filename_to_put, strlen(filename_to_put) , 0 , (struct sockaddr *) &sock_serv, slen)==-1)
	{
		error("sendto()");
	}
	fd = open(filename_to_put, O_RDONLY);
	if (fd != -1) {
		if (stat(filename_to_put, &buffer) == -1)
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

		unlock_server();
		printf("Numero byte trasferiti : %zd\n", count);
		printf("Su un numero totale di : %zd \n", sz);

		close(fd);
	}
	else{
		perror("open file fail");
		unlock_server();
	}
}
int main(void) {

	int exitwhile;
	int pid;
	char message[BUFFLEN];


	if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		error("socket");
	}

	memset((char *) &sock_serv, 0, sizeof(sock_serv));
	sock_serv.sin_family = AF_INET;
	sock_serv.sin_port = htons(PORT);

	if (inet_aton(SERVER, &sock_serv.sin_addr) == 0) {
		error("inet_aton() failed");
	}

	while (1) {
		exitwhile = 0;
		while (exitwhile == 0) {
			memset(message, 0, BUFFLEN);
			printf("\nWhat you want to do? \n");
			if (scanf("%s", message) == -1)
				error("scanf");

			if (strcmp(message, "list") == 0 || strcmp(message, "get") == 0 || strcmp(message, "put") == 0)
				exitwhile = 1;

		}
		//send the message
		if (sendto(sfd, message, strlen(message), 0, (struct sockaddr *) &sock_serv, slen) == -1) {
			error("sendto()");
		}
		printf("richiesta preliminare -- porta server:%d \n",sock_serv.sin_port);
		if((pid=fork())==-1)
		{
			error("fork fail");
		}
		if(pid == 0) {
			recvfrom(sfd, "", 0, 0, (struct sockaddr *) &sock_serv, &slen);
			if (strncmp(message, "list", 4) == 0) {
				list_from_server();
			}

			if (strncmp(message, "get", 3) == 0) {
				get_data_from_server();
			}

			if (strncmp(message, "put", 3) == 0) {
				put_file_to_server();
			}
			exit(0);
		}
		wait(NULL);
	}

}

