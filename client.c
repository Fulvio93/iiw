
#include "header.h"

struct sockaddr_in sock_serv;
int sfd;
socklen_t slen=sizeof(sock_serv);

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
	char filename_to_get[BUFFLEN];
	ssize_t bytesread,byteswritten;
	int contascartati=0,contabuf=0,contascritti=0;
	int send_base=0;
	int position_in_sequence_array;
	int p,j;
	off_t filesize;
	struct udp_pkt_s udp_pkt[(2*W)];
	struct udp_pkt_s rcv_udp_pkt;
	pid_t pid;
	bool ricevuto[2*W];
	struct timespec time_s;
	time_s.tv_sec = SECTIMEOUT;
	time_s.tv_nsec = NSECTIMEOUT;

	fprintf(stdout,"Write the name of the file you want download: ");
	if(scanf("%s", filename_to_get)==-1)
		error("scanf");
	if (sendto(sfd, filename_to_get, strlen(filename_to_get) , 0 , (struct sockaddr *) &sock_serv, slen)==-1)
	{
		error("sendto()");
	}
	if(recvfrom(sfd, &filesize, sizeof(filesize), 0, (struct sockaddr *) &sock_serv, &slen) == -1)
	{
		error("recvfrom");
	}
	printf("Dimensione file: %zd\n",filesize);
	fflush(stdout);
	fd = open(filename_to_get,O_CREAT|O_WRONLY|O_TRUNC,0600);

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
			}
			else {
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
				ricevuto[rcv_udp_pkt.seq] = 0;


				for (j = 0; j < W; j++) {    //ciclo che calcola la nuova send_base, controllando i pacchetti con ack = 1
					position_in_sequence_array = send_base + 1 + j;
					if (position_in_sequence_array > max_num_seq) // se vero, ho sforato il num max di sequenza
					{
						position_in_sequence_array = (position_in_sequence_array - max_num_seq) - 1;
					}
					if (ricevuto[position_in_sequence_array] ==	0) { // appena trova il primo pacchetto con ack=0, aggiornerà la posizione della send_base
						send_base = position_in_sequence_array;// aggiornerà la posizione della send_base
						break;                                          //fermo il ciclo perchè ho trovato la nuova send_base
					}
					else {
						byteswritten = write(fd, udp_pkt[position_in_sequence_array].buf, udp_pkt[position_in_sequence_array].bytesletti);
						if (byteswritten == -1)
							error("write file fail");
						contascritti += byteswritten;
						ricevuto[position_in_sequence_array] = 0;
					}
				}
			}
		}

		if(contascritti==filesize ){
			unlock_server();
			if((pid=fork()) == 0)
			{
				while(1) {
					if (recvfrom(sfd, &rcv_udp_pkt, sizeof(rcv_udp_pkt), 0, (struct sockaddr *) &sock_serv, &slen) ==
						-1) {
						error("recvfrom");
					}
				}
			}
			else{
				if (nanosleep(&time_s, NULL) == -1)
					error("nanosleep");
				kill(pid,SIGKILL);
			}

			puts("finito di ricevere");
			break;
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
	int fd;
	char filename[128];
	off_t sz;
	int l = sizeof(struct sockaddr_in);
	struct stat buffer;
	ssize_t recv_len = 1;
	ssize_t bytesread = 1;
	int num_seq;
	int slots_occupati_finestra=0;
	int send_base;
	int j;
	int position_in_sequence_array;
	int max_num_seq = (2 * W) - 1;
	pid_t pid[2 * W];
	int p;
	struct udp_pkt_s udp_pkt[(2 * W)];
	struct udp_pkt_s rcv_udp_pkt;
	struct timespec time_s;
	time_s.tv_sec = SECTIMEOUT;
	time_s.tv_nsec = NSECTIMEOUT;
	pid_t sid;
	int check_num_seq = 0;
	bool timerfree=1;
	unsigned long long sampleRTT, estimatedRTT = 1000000 * time_s.tv_sec + time_s.tv_nsec / 1000;
	if(( sid = setsid())==-1)
	{
		error("setsid");
	}
	printf("Insert file name... ");
	if((scanf("%s",filename)) == -1)
	{
		error("scanf");
	}

	if (sendto(sfd, &filename, sizeof(filename), 0, (struct sockaddr *) &sock_serv, l) == -1) {
		error("sendto");
	}

	fd = open(filename, O_RDONLY);
	if (fd != -1) {

		//dimensione del file
		if (stat(filename, &buffer) == -1)
			error("stat fail");
		else
			sz = buffer.st_size;

		printf("Dimensione file: %zd\n", sz);
		fflush(stdout);

		if (sendto(sfd, &sz, sizeof(sz), 0, (struct sockaddr *) &sock_serv, l) == -1) {
			error("sendto");
		}


		num_seq = 0;
		slots_occupati_finestra = 0;
		send_base = 0;
		srand(time(NULL));

		while (recv_len) { //finchè non raggiungo la fine del file
			while (slots_occupati_finestra < W) {
				memset(&udp_pkt[num_seq], 0, sizeof(udp_pkt[num_seq]));
				bytesread = read(fd, udp_pkt[num_seq].buf, BUFFLEN); //leggo buffer del file
				if (bytesread > 0) {

					//creazione pacchetto da inviare

					udp_pkt[num_seq].ack = 0;
					udp_pkt[num_seq].bytesletti = bytesread;
					udp_pkt[num_seq].seq = num_seq;


					//fine creazione pacchetto da inviare
					p = rand() % 100 + 1;
					if (p > PROB) {
						if (sendto(sfd, &udp_pkt[num_seq], sizeof(udp_pkt[num_seq]), 0, (struct sockaddr *) &sock_serv,
								   l) == -1) {
							error("sendto");
						}
						if(timerfree && ADAPTIVE_TIMEOUT) {
							gettimeofday(&start, NULL);
							check_num_seq = num_seq;
							timerfree = 0;
						}

					} /*else {
						printf("pacchetto #seq: %d SCARTATO!\n", udp_pkt[num_seq].seq);
						fflush(stdout);
					}*/


					memset(&pid[num_seq], 0, sizeof(int));
					pid[num_seq] = fork();

					if (pid[num_seq] == -1) {
						error("fork");
					}
					if (pid[num_seq] == 0) //sono il figlio
					{
						setpgid(0,getppid());
						while (1) {

							if (nanosleep(&time_s, NULL) == -1)
								exit(0);
							p = rand() % 100 + 1;
							if (p > 10) {
								if (sendto(sfd, &udp_pkt[num_seq], sizeof(udp_pkt[num_seq]), 0,
										   (struct sockaddr *) &sock_serv, l) == -1) {
									error("sendto");
								}
							}
						}
					}
				} else {

					slots_occupati_finestra = 2*W; //dato che ho finito di leggere incremento gli slot a tal punto da non trasmettere piu

				}
				if (num_seq < max_num_seq) {
					num_seq++;
					udp_pkt[num_seq].seq = num_seq;
				}
				else {
					num_seq = 0;
					udp_pkt[num_seq].seq = 0;
				};

				slots_occupati_finestra++;
			}

			while (1) {
				puts("FINESTRA PIENA!! ATTESA DI ACK");

				memset(&rcv_udp_pkt, 0, sizeof(rcv_udp_pkt));
				recv_len = recvfrom(sfd, &rcv_udp_pkt, sizeof(struct udp_pkt_s), 0, (struct sockaddr *) &sock_serv,
									&slen);
				if (recv_len == -1) {
					error("recvfrom()");
				}
				if (recv_len == 0) {
					close(fd);
					kill(-getpid(), SIGKILL); //se ho finito di ricevere, uccido tutti

				}
				if (rcv_udp_pkt.ack ==	1) {   //se nel pacchetto ricevuto l'ack è uguale a 1 allora vuol dire che il destinatario l'ha ricevuto correttamente
					kill(pid[rcv_udp_pkt.seq], SIGKILL); //uccido il processo timer legato al numero di sequenza dell'ACK ricevuto

					if((rcv_udp_pkt.seq == check_num_seq) && ADAPTIVE_TIMEOUT)
					{
						gettimeofday(&end,NULL);
						sampleRTT = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
						estimatedRTT = 0.875 * estimatedRTT + 0.125 * sampleRTT;
						time_s.tv_sec = estimatedRTT / 1000000;
						time_s.tv_nsec = (estimatedRTT * 1000) - (time_s.tv_sec * 1000000000);
						printf("Sample: %llu\n",sampleRTT);
						printf("Estimated: %llu\n",estimatedRTT);

						timerfree = 1;
					}
					udp_pkt[rcv_udp_pkt.seq].ack = 1;


					if (rcv_udp_pkt.seq == send_base) {  // se l'ack ricevuto ha numero di sequenza uguale alla send_base, allora libero uno slot della finestra e aggiorno la send_base


						for (j = 0; j <
									W; j++) {    //ciclo che calcola la nuova send_base, controllando i pacchetti con ack = 1
							position_in_sequence_array = send_base + j + 1;
							if (position_in_sequence_array > max_num_seq) // se vero, ho sforato il num max di sequenza
							{
								position_in_sequence_array = (position_in_sequence_array - max_num_seq) - 1;

							}
							if (j == (W - 1)) {
								send_base = position_in_sequence_array;// aggiornerà la posizione della send_base
								slots_occupati_finestra = slots_occupati_finestra - (j + 1);  // e libererà tanti slot dell finestra quante sono le iterazioni che ha fatto

								break;
							}
							if (udp_pkt[position_in_sequence_array].ack ==
								0) { // appena trova il primo pacchetto con ack=0, aggiornerà la posizione della send_base
								send_base = position_in_sequence_array;// aggiornerà la posizione della send_base
								slots_occupati_finestra = slots_occupati_finestra - (j +
																					 1);  // e libererà tanti slot dell finestra quante sono le iterazioni che ha fatto

								break;                                          //fermo il ciclo perchè ho trovato la nuova send_base
							}
						}

						break;//se cambia la send_base posso interrompere il ciclo di ascolto e ricominciare a inviare
					}
				}
			}
		}
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
			close(sfd);
			exit(0);
		}
		wait(NULL);
	}

}

