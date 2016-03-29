#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<unistd.h>
#include<time.h>
#include<stdbool.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/wait.h>


#define SERVER "127.0.0.1"
#define BUFFLEN 512 //Max length of buffer
#define PORT 8888   //The port on which to send data
#define W 8

struct sockaddr_in sock_serv;
int sfd;
int max_num_seq = (2*W)-1;
socklen_t slen=sizeof(sock_serv);
struct udp_pkt_s{
	int seq;
	bool ack;
	char buf[BUFFLEN];
	int bytesletti;
};

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

bool check_if_in_window(int seq, int base)
{
	if((base + W - 1) > max_num_seq) {
		if (((seq >= base) && (seq <= max_num_seq)) || ((seq >= 0) && (seq <= (base + W - 1 - max_num_seq - 1))))
			return 1;
		else
			return 0;
	}else{
		if(seq >= base && seq < base + W)
			return 1;
		else
			return 0;
	}
}
void get_data_from_server()
{
	printf("inizio operazione get -- porta server:%d\n",sock_serv.sin_port);
	ssize_t bytesread,byteswritten;
	int contascartati=0,contabuf=0,contascritti=0;
	int fd, send_base=0;
	int position_in_sequence_array;
	char filename[128];
	int p,j;
	off_t filesize;
	struct udp_pkt_s udp_pkt[(2*W)];
	struct udp_pkt_s rcv_udp_pkt;
	bool ricevuto[2*W];


	puts("Insert the name of the file you want transfer: ");
	if (scanf("%s", filename) == -1)
		error("scanf");

	puts(rcv_udp_pkt.buf);

	memset(ricevuto,0,sizeof(ricevuto));

	if (sendto(sfd, filename, sizeof(filename), 0, (struct sockaddr *) &sock_serv, slen) == -1) {
		error("sendto()");
	}

	if(recvfrom(sfd, &filesize, sizeof(filesize), 0, (struct sockaddr *) &sock_serv, &slen) == -1)
	{
		error("recvfrom");
	}
	printf("Dimensione file: %zd\n",filesize);
	fflush(stdout);
	fd = open(filename,O_CREAT|O_WRONLY|O_TRUNC,0600);

	if((fd==-1)){
		error("open fail");
	}
	srand(time(NULL));
	while(1) {
		memset(&rcv_udp_pkt, 0, sizeof(rcv_udp_pkt));
		bytesread = recvfrom(sfd, &rcv_udp_pkt, sizeof(rcv_udp_pkt), 0, (struct sockaddr *) &sock_serv, &slen);
		if (bytesread == -1) {
			error("recvfrom");
		}

		printf("ricevuto buf %d... #seq = %d\n", contabuf, rcv_udp_pkt.seq);
		contabuf++;

		p = rand() % 100 + 1;

		if(rcv_udp_pkt.seq!=-1){
			if (p > 10) {
				rcv_udp_pkt.ack = 1;

				if (sendto(sfd, &rcv_udp_pkt, sizeof(rcv_udp_pkt), 0, (struct sockaddr *) &sock_serv, slen) == -1) {
					error("sendto()");
				}
				printf("mandato ack per #seq; %d\n", rcv_udp_pkt.seq);
			}
			else {
				puts("Ack scartato!");
				contascartati++;
			}
		}



		if (check_if_in_window(rcv_udp_pkt.seq, send_base))
		{
			if (ricevuto[rcv_udp_pkt.seq] == 0) {
				memset(&udp_pkt[rcv_udp_pkt.seq], 0, sizeof(udp_pkt[rcv_udp_pkt.seq]));
				udp_pkt[rcv_udp_pkt.seq] = rcv_udp_pkt;
				ricevuto[rcv_udp_pkt.seq] = 1;
			}

			if (rcv_udp_pkt.seq == send_base) {

				byteswritten = write(fd, rcv_udp_pkt.buf, rcv_udp_pkt.bytesletti);
				if (byteswritten == -1)
					error("write file fail");
				contascritti += byteswritten;
				printf("Scritto buf #seq: %d\n", rcv_udp_pkt.seq);
				ricevuto[rcv_udp_pkt.seq] = 0;

				printf("bisogna aggiornare la send_base! send base corrente:%d\n", send_base);
				fflush(stdout);

				for (j = 0; j < W; j++) {    //ciclo che calcola la nuova send_base, controllando i pacchetti con ack = 1
					position_in_sequence_array = send_base + 1 + j;
					if (position_in_sequence_array > max_num_seq) // se vero, ho sforato il num max di sequenza
					{
						printf("posizione di sforamento=%d\n", position_in_sequence_array);
						position_in_sequence_array = (position_in_sequence_array - max_num_seq) - 1;
						printf("nuova posizione %d\n", position_in_sequence_array);
					}
					if (ricevuto[position_in_sequence_array] ==
						0) { // appena trova il primo pacchetto con ack=0, aggiornerà la posizione della send_base
						printf("il primo pacchetto non ricevuto dopo la send_base è il numero:%d\n",
							   position_in_sequence_array);
						send_base = position_in_sequence_array;// aggiornerà la posizione della send_base
						printf("send_base aggiornata = %d\n", send_base);
						fflush(stdout);
						break;                                          //fermo il ciclo perchè ho trovato la nuova send_base
					}
					else {
						byteswritten = write(fd, udp_pkt[position_in_sequence_array].buf, udp_pkt[position_in_sequence_array].bytesletti);
						if (byteswritten == -1)
							error("write file fail");
						contascritti += byteswritten;
						printf("Scritto buf #seq: %d\n", position_in_sequence_array);
						ricevuto[position_in_sequence_array] = 0;
					}
				}
			}
		}

		if (rcv_udp_pkt.seq==-1) {
			if(contascritti==filesize ){
				unlock_server();
				puts("finito di ricevere");
				break;
			}
		}
	}
	printf("bytes scritti: %d\n",contascritti);
	printf("scartati: %d\n",contascartati);

	close(fd);
	puts("esco! ciao");

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

