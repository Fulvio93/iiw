#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>
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
int main(void)
{

	int exitwhile;
	char buf[BUFFLEN];
	char message[BUFFLEN];
	char filenameget[BUFFLEN];
	int fd;
	ssize_t bytesread,byteswritten, count = 0;

	if ( (sfd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		error("socket");
	}

	memset((char *) &sock_serv, 0, sizeof(sock_serv));
	sock_serv.sin_family = AF_INET;
	sock_serv.sin_port = htons(PORT);

	if (inet_aton(SERVER , &sock_serv.sin_addr) == 0)
	{
		error("inet_aton() failed");
	}

	while(1)
	{	exitwhile = 0;
		while(exitwhile == 0)
		{	memset(message,0, BUFFLEN);
			printf("\nWhat you want to do? ");
			if(scanf("%s", message)==-1)
				error("scanf");

			if(strcmp(message,"list") == 0||strcmp(message,"get") == 0||strcmp(message,"put") == 0)
				exitwhile = 1;

		}
		//send the message
		if (sendto(sfd, message, strlen(message) , 0 , (struct sockaddr *) &sock_serv, slen)==-1)
		{
			error("sendto()");
		}


		if(strncmp(message,"list",4) == 0){
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

		if(strncmp(message,"get",3) == 0)
		{
			fprintf(stdout,"Write the name of the file you want download: ");
			if(scanf("%s", filenameget)==-1)
				error("scanf");
			if (sendto(sfd, filenameget, strlen(filenameget) , 0 , (struct sockaddr *) &sock_serv, slen)==-1)
			{
				error("sendto()");
			}
			fd = open("copia",O_CREAT|O_WRONLY|O_TRUNC,0600);

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
				if(byteswritten==-1)
					error("write file fail");

				memset(buf,0,BUFFLEN);
				bytesread = recvfrom(sfd, buf, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen);
			}
			printf("Numero byte ricevuti : %zd\n",count);
			close(fd);
		}



		if(strncmp(message,"put",3) == 0){
			fprintf(stdout,"Write a name of file you want upload: ");
			if(scanf("%s", message)==-1)
				error("scanf");

		}

	}

}
