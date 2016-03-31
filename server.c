#include "header.h"


char path[256];
struct sockaddr_in sock_serv, clnt;
int sfd;
socklen_t slen = sizeof(clnt);
DIR *dp;
struct dirent *ep;

void unlock_client()
{
	if (sendto(sfd, "", 0, 0, (struct sockaddr*) &clnt, slen) == -1)
	{
		error("sendto()");
	}
}
void list_elements_to_client()
{
	printf("inizio operazione list -- porta server:%d verso porta client: %d\n",sock_serv.sin_port,clnt.sin_port);

	dp = opendir (path);

	while ((ep = readdir(dp)))
	{
		if (sendto(sfd, ep->d_name, BUFFLEN, 0, (struct sockaddr*) &clnt, slen) == -1)
		{
			error("sendto()");
		}
	}

	(void) closedir (dp);
}
void send_data_to_client() {
	printf("inizio operazione get -- porta server:%d verso porta client: %d\n", sock_serv.sin_port, clnt.sin_port);

	int fd;
	char file_request[BUFFLEN] = "";
	off_t sz;
	int l = sizeof(struct sockaddr_in);
	struct stat buffer;
	char getbuf[BUFFLEN];
	ssize_t recv_len = 1;
	ssize_t bytesread = 1;
	int num_seq;
	int pacchetti_scartati = 0;
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
	puts("GET request");
	puts("Waiting for file name... ");

	memset(getbuf, 0, BUFFLEN);
	bytesread = recvfrom(sfd, getbuf, BUFFLEN, 0, (struct sockaddr *) &sock_serv, &slen);

	if (bytesread == -1) {
		error("recvfrom()");
	}

	strcat(file_request, path);
	strcat(file_request, getbuf);

	fd = open(file_request, O_RDONLY);
	if (fd != -1) {

		//dimensione del file
		if (stat(file_request, &buffer) == -1)
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
					if (p > 10) {
						if (sendto(sfd, &udp_pkt[num_seq], sizeof(udp_pkt[num_seq]), 0, (struct sockaddr *) &sock_serv,
								   l) == -1) {
							error("sendto");
						}
						if(timerfree) {
							gettimeofday(&start, NULL);
							check_num_seq = num_seq;
							timerfree = 0;
						}

					} else {
						pacchetti_scartati++;
					}


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

					if(rcv_udp_pkt.seq == check_num_seq)
					{
						gettimeofday(&end,NULL);
						sampleRTT = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
						estimatedRTT = 0.875 * estimatedRTT + 0.125 * sampleRTT;
						time_s.tv_sec = estimatedRTT / 1000000;
						time_s.tv_nsec = (estimatedRTT * 1000) - (time_s.tv_sec * 1000000000);
						printf("Sample: %llu\n",sampleRTT);
						printf("Estimated: %llu\n",estimatedRTT);
						printf("new timer sec: %ld\n",time_s.tv_sec);
						printf("new timer nanosec: %ld\n",time_s.tv_nsec);

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
							if (udp_pkt[position_in_sequence_array].ack == 0) { // appena trova il primo pacchetto con ack=0, aggiornerà la posizione della send_base
								send_base = position_in_sequence_array;// aggiornerà la posizione della send_base
								slots_occupati_finestra = slots_occupati_finestra - (j + 1);  // e libererà tanti slot dell finestra quante sono le iterazioni che ha fatto
								break;                                          //fermo il ciclo perchè ho trovato la nuova send_base
							}
						}

						break;//se cambia la send_base posso interrompere il ciclo di ascolto e ricominciare a inviare
					}
				}
			}
		}
	}
	else{
		error("open");
	}
}
void receive_data_from_client()
{
	printf("inizio operazione put -- porta server:%d verso porta client: %d\n",sock_serv.sin_port,clnt.sin_port);
	int fd;
	char filename_to_get[BUFFLEN];
	char file_request[BUFFLEN] ="";
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

	if(recvfrom(sfd, &filename_to_get, sizeof(filename_to_get), 0, (struct sockaddr *) &sock_serv, &slen) == -1)
	{
		error("recvfrom");
	}
	if(recvfrom(sfd, &filesize, sizeof(filesize), 0, (struct sockaddr *) &sock_serv, &slen) == -1)
	{
		error("recvfrom");
	}
	printf("Dimensione file: %zd\n",filesize);
	fflush(stdout);

	strcat(file_request, path);
	strcat(file_request, filename_to_get);

	fd = open(file_request,O_CREAT|O_WRONLY|O_TRUNC,0600);

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
					if (ricevuto[position_in_sequence_array] == 0) { // appena trova il primo pacchetto con ack=0, aggiornerà la posizione della send_base
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
			unlock_client();
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

			printf("Received packet from: %s, port: %d\n", inet_ntoa(clnt.sin_addr), ntohs(clnt.sin_port));

			if ((sfd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
			{
				error("socket");
			}

			// zero out the structure
			memset((char *) &sock_serv, 0, sizeof(sock_serv));

			sock_serv.sin_family = AF_INET;
			sock_serv.sin_port = htons(0);
			sock_serv.sin_addr.s_addr = htonl(INADDR_ANY);

			//bind socket to port
			if( bind(sfd , (struct sockaddr*)&sock_serv, sizeof(sock_serv) ) == -1)
			{
				error("bind");
			}
			unlock_client(); //invio al client un pacchetto vuoto ma che contiene le informazioni sulla nuova porta

			parse_data(buf);
			close(sfd);
			exit(0);
		}
		usleep(1000); //faccio partire prima il figlio
	}
}
