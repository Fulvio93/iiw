
#include "header.h"


char path[256];
struct sockaddr_in sock_serv, clnt;
int sfd;
socklen_t slen = sizeof(clnt);
DIR *dp;
struct dirent *ep;


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
	printf("inizio operazione list -- porta server:%d verso porta client: %d\n",sock_serv.sin_port,clnt.sin_port);

	dp = opendir (path);

	while ((ep = readdir(dp)))
	{
		sendbuf(ep->d_name);
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
	int slots_occupati_finestra=0;
	int send_base;
	int j;
	int ritrasmissioni = 0;
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

	printf("file richiesto: %s\n", file_request);

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
				printf("bytesread: %zd\n", bytesread);
				fflush(stdout);
				if (bytesread > 0) {

					//creazione pacchetto da inviare

					udp_pkt[num_seq].ack = 0;
					udp_pkt[num_seq].bytesletti = bytesread;
					printf("#seq: %d\n", num_seq);
					fflush(stdout);
					udp_pkt[num_seq].seq = num_seq;


					//fine creazione pacchetto da inviare
					p = rand() % 100 + 1;
					if (p > 10) {
						if (sendto(sfd, &udp_pkt[num_seq], sizeof(udp_pkt[num_seq]), 0, (struct sockaddr *) &sock_serv,
								   l) == -1) {
							error("sendto");
						}
						printf("pacchetto #seq: %d inviato! timer partito!\n", udp_pkt[num_seq].seq);
						fflush(stdout);

					} else {
						printf("pacchetto #seq: %d SCARTATO!\n", udp_pkt[num_seq].seq);
						fflush(stdout);
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
							printf("TIMER SCADUTO! #seq: %d ... RINVIO!\n", udp_pkt[num_seq].seq);
							fflush(stdout);
							p = rand() % 100 + 1;
							if (p > 10) {
								if (sendto(sfd, &udp_pkt[num_seq], sizeof(udp_pkt[num_seq]), 0,
										   (struct sockaddr *) &sock_serv, l) == -1) {
									error("sendto");
								}
								printf("pacchetto #seq: %d inviato! timer partito!\n", udp_pkt[num_seq].seq);
								fflush(stdout);
							} else {
								printf("pacchetto #seq: %d SCARTATO!\n", udp_pkt[num_seq].seq);
								fflush(stdout);

							}
							ritrasmissioni++;
							printf("ritrasmissione numero:%d\n", ritrasmissioni);
							fflush(stdout);
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
				printf("SLOT OCCUPATI: %d\n", slots_occupati_finestra);
				fflush(stdout);
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
					puts("il client mi ha mandato un pacchetto vuoto");
					close(fd);
					kill(-getpid(), SIGKILL); //se ho finito di ricevere, uccido tutti

				}
				printf("ack ricevuto!! sono il processo padre: %d\n", getpid());
				printf("ho ricevuto il pacchetto con #seq:%d, e con ack=%d\n", rcv_udp_pkt.seq, rcv_udp_pkt.ack);
				if (rcv_udp_pkt.ack ==	1) {   //se nel pacchetto ricevuto l'ack è uguale a 1 allora vuol dire che il destinatario l'ha ricevuto correttamente
					kill(pid[rcv_udp_pkt.seq], SIGKILL); //uccido il processo timer legato al numero di sequenza dell'ACK ricevuto

					udp_pkt[rcv_udp_pkt.seq].ack = 1;

					printf("ucciso il processo: %d\n", pid[rcv_udp_pkt.seq]);

					if (rcv_udp_pkt.seq == send_base) {  // se l'ack ricevuto ha numero di sequenza uguale alla send_base, allora libero uno slot della finestra e aggiorno la send_base

						printf("bisogna aggiornare la send_base!  send base corrente:%d\n", send_base);
						fflush(stdout);

						for (j = 0; j <
									W; j++) {    //ciclo che calcola la nuova send_base, controllando i pacchetti con ack = 1
							position_in_sequence_array = send_base + j + 1;
							if (position_in_sequence_array > max_num_seq) // se vero, ho sforato il num max di sequenza
							{
								printf("posizione di sforamento=%d\n", position_in_sequence_array);
								position_in_sequence_array = (position_in_sequence_array - max_num_seq) - 1;
								printf("nuova posizione %d\n", position_in_sequence_array);

							}
							if (j == (W - 1)) {
								printf("il primo pacchetto senza ack dopo la send_base è il numero:%d\n",
									   position_in_sequence_array);
								fflush(stdout);
								send_base = position_in_sequence_array;// aggiornerà la posizione della send_base
								printf("base aggiornata, la nuova base è %d\n", send_base);
								fflush(stdout);
								slots_occupati_finestra = slots_occupati_finestra - (j +
																					 1);  // e libererà tanti slot dell finestra quante sono le iterazioni che ha fatto
								printf("sbloccati %d slot della finestra\n", (j + 1));
								fflush(stdout);

								break;
							}
							if (udp_pkt[position_in_sequence_array].ack ==
								0) { // appena trova il primo pacchetto con ack=0, aggiornerà la posizione della send_base
								printf("il primo pacchetto senza ack dopo la send_base è il numero:%d\n",
									   position_in_sequence_array);
								fflush(stdout);
								send_base = position_in_sequence_array;// aggiornerà la posizione della send_base
								printf("base aggiornata, la nuova base è %d\n", send_base);
								fflush(stdout);
								slots_occupati_finestra = slots_occupati_finestra - (j +
																					 1);  // e libererà tanti slot dell finestra quante sono le iterazioni che ha fatto
								printf("sbloccati %d slot della finestra\n", (j + 1));
								fflush(stdout);

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
void receive_data_from_client()
{
	printf("inizio operazione put -- porta server:%d verso porta client: %d\n",sock_serv.sin_port,clnt.sin_port);
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
