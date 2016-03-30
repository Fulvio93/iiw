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

#define BUFFLEN 10240 //Max length of buffer
#define PORT 8888   //The port on which to listen for incoming data
#define W 16
#define SECTIMEOUT 0
#define NSECTIMEOUT 10000000

int max_num_seq = (2*W)-1;

struct udp_pkt_s{
    int seq;
    bool ack;
    char buf[BUFFLEN];
    int bytesletti;
};

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
void error(char *str)
{
    perror(str);
    exit(1);
}