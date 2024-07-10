#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#define BUFFER_SIZE 1500
#define SEGMENT_DATA_SIZE 1400

struct sockaddr_in si_me, si_other;
//int s;
//socket descriptor, size of the socket
int socket_descriptor;


// TCP header structure
    struct tcp_segment {
        // unsigned short int src_port;//16 bits
        // unsigned short int dest_port;//16 bits
        unsigned int seq_num;//32 bits
        unsigned int ack_num;//32 bits
        unsigned short int syn;//lag：Synchronize Sequence Numbers
        unsigned short int fin;//flag：Acknowledgment
        int data_length; //length of data written in the seg
        char data[SEGMENT_DATA_SIZE + 1];//
    };
void diep(char *s) {
    perror(s);
    exit(1);
}

void create_ack(struct tcp_segment *ack_header, unsigned int seq_num, unsigned int ack_num, unsigned short int syn, unsigned short int fin){
    ack_header->seq_num = seq_num;
    ack_header->ack_num = ack_num;
    ack_header->syn = syn;
    ack_header->fin = fin;
    ack_header->data[0] = '\0';
    ack_header->data_length = 0;
}
// Global variables for timer
struct timeval last_ack_time;
#define TIMER_INTERVAL_MS 100 // 50ms
// 检查是否到了重传ACK的时间
int is_time_to_resend_ack() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);//获取当前时间
    int elapsed_ms = (current_time.tv_sec - last_ack_time.tv_sec) * 1000 +
                     (current_time.tv_usec - last_ack_time.tv_usec) / 1000;
    return elapsed_ms >= TIMER_INTERVAL_MS;
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    //char buf[BUFFER_SIZE];
    FILE *fp;
    int recv_len;
    //socklen_t slen = sizeof(si_other);
    socklen_t socket_size = sizeof(si_other);
    unsigned int server_isn;
    if ((socket_descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(socket_descriptor, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");
    int next_seq_expected = 0;//the next seq_num the server expected to receive
	/* Now receive data and send acknowledgements */  
    //setting up the connection
    while(1){
        int state = 0;//0: LISTEN, 1: SYN-RCVD, 2: Established
        printf("Waiting for connections\n");
        //1. receive SYN from sender
        struct tcp_segment syn_header;
        if ((recv_len = recvfrom(socket_descriptor, &syn_header, sizeof(syn_header), 0, (struct sockaddr *) &si_other, &socket_size)== -1)){
            diep("recvfrom()");
        }
        //if syn is correct, send syn_ack,set state to SYN-RCVD
        struct tcp_segment syn_ack_header;
        if (syn_header.syn == 1){
            //randomly select server initial sequence number 
            srand(getpid());//根据进程数设置随机数种子    
            server_isn = random(); // 生成随机数
            create_ack(&syn_ack_header, server_isn, syn_header.seq_num + 1, 1, 0);
            if(sendto(socket_descriptor, &syn_ack_header, sizeof(syn_ack_header), 0, (struct sockaddr *)&si_other, socket_size) == -1){
                diep("sendto()");
            }
            else{
                printf("SYN-ACK sent\n");
                state = 1;
            }
        }
        else{
            printf("SYN is not correct\n");
            continue;
        }
        //state: SYN-RCVD
        // 2. receive ack segment
        struct tcp_segment ack_header;
        if ((recv_len = recvfrom(socket_descriptor, &ack_header, sizeof(ack_header), 0, (struct sockaddr *) &si_other, &socket_size)== -1)){
            diep("recvfrom()");
        }
        //if ack is correct, set up connection, set state to Established
        if(ack_header.syn == 0 && ack_header.seq_num == syn_ack_header.ack_num && ack_header.ack_num == server_isn + 1){
            printf("Connection established\n");
            state = 2;
            next_seq_expected = ack_header.seq_num + 1;
        }
        else{
            printf("ACK is not correct");
            continue;
        }
        //state: Established
        if (state == 2){
            break;
        }
    }
    printf("starting to receive data");
    //create a file to write the data
    if (!(fp = fopen(destinationFile, "wb"))) {
        diep("fopen");
    }
    //receive data
    struct tcp_segment segment_received;//cuurent segment received
    struct tcp_segment ack_header;//ack segment to send
    struct tcp_segment fin_ack_header; //fin_ack segment to send
    //timer 
    //TODO: set up the timer
    gettimeofday(&last_ack_time, NULL);
    while (1) {
        if ((recv_len = recvfrom(socket_descriptor, &segment_received, sizeof(segment_received), 0, (struct sockaddr *) &si_other, &socket_size)== -1)){
            diep("recvfrom()");
        }
        // there are five conditions:
        //1.  if receive fin, send ack break
        if (segment_received.fin == 1) {
            create_ack(&fin_ack_header, segment_received.ack_num, segment_received.seq_num + 1, 0, 1);
            if(sendto(socket_descriptor, &fin_ack_header, sizeof(fin_ack_header), 0, (struct sockaddr *)&si_other, socket_size) == -1){
                diep("sendto()");
            }
            printf("file received\n");
            break;
        }
        //2. 具有所期望序号的按序报文段到达。所有在期望序号及以前的数据都已经被确认
        if(segment_received.seq_num == next_seq_expected){
            //write the data to the file
            fwrite(segment_received.data, 1, segment_received.data_length, fp);
            //send ack 
            create_ack(&ack_header, segment_received.ack_num, segment_received.seq_num + SEGMENT_DATA_SIZE, 0, 0);
            if(sendto(socket_descriptor, &ack_header, sizeof(ack_header), 0, (struct sockaddr *)&si_other, socket_size) == -1){
                diep("sendto()");
            }   
            next_seq_expected = segment_received.seq_num + SEGMENT_DATA_SIZE;
            //start new timer for the ack
            gettimeofday(&last_ack_time, NULL);
        }
        // 3. 比期望序号大的失序报文段到达，检测出gap
        else if(segment_received.seq_num > next_seq_expected){
            //send ack with the last correct ack number
            create_ack(&ack_header, segment_received.ack_num, next_seq_expected, 0, 0);
            if(sendto(socket_descriptor, &ack_header, sizeof(ack_header), 0, (struct sockaddr *)&si_other, socket_size) == -1){
                diep("sendto()");
            }   
        }
        // 4. 重复的报文段到达
        else{ 
            //(segment_received.seq_num < next_seq_expected)
            //send ack with the last correct ack number
            create_ack(&ack_header, segment_received.ack_num, next_seq_expected, 0, 0);
            if(sendto(socket_descriptor, &ack_header, sizeof(ack_header), 0, (struct sockaddr *)&si_other, socket_size) == -1){
                diep("sendto()");
            }   
        }
        // 5. 检查是否到了重传ACK的时间
        if (is_time_to_resend_ack()) {
            if(sendto(socket_descriptor, &ack_header, sizeof(ack_header), 0, (struct sockaddr *)&si_other, socket_size) == -1){
                diep("sendto()");
            }   
            //start new timer
            gettimeofday(&last_ack_time, NULL);
        }
    }
    //close the connection
    //send fin
    struct tcp_segment fin_server;
    create_ack(&fin_server, segment_received.ack_num, segment_received.seq_num + 1, 0, 1);
    if(sendto(socket_descriptor, &fin_server, sizeof(fin_server), 0, (struct sockaddr *)&si_other, socket_size) == -1){
        diep("sendto()");
    }
    //receive fin_ack
    while(1){
        struct tcp_segment fin_ack;
        if ((recv_len = recvfrom(socket_descriptor, &fin_ack, sizeof(fin_ack), 0, (struct sockaddr *) &si_other, &socket_size)== -1)){
            diep("recvfrom()");
        }
        //if the ack segment is correct, break
        if(fin_ack.ack_num == fin_server.seq_num + 1){
            printf("Connection closed\n");
            break;
        }
        else{
            printf("fin_ack is not correct");
        }
    }
    fclose(fp);
    close(socket_descriptor);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
    
}

