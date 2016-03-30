#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<dirent.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<stdbool.h>
#include<time.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include<signal.h>

#define SERVER "127.0.0.1"

#define BUFFLEN 20480 //Max length of buffer
#define PORT 8888   //The port on which to listen for incoming data
#define W 8
#define SECTIMEOUT 0
#define NSECTIMEOUT 100000000
int max_num_seq = (2*W)-1;



struct udp_pkt_s{
    int seq;
    bool ack;
    char buf[BUFFLEN];
    int bytesletti;
};

void error(char *str)
{
    perror(str);
    exit(1);
}