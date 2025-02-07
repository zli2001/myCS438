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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#define BUFFER_SIZE 1500
#define SEGMENT_DATA_SIZE 1400
struct sockaddr_in si_other;
//socket descriptor, size of the socket
int socket_descriptor;
char buffer[BUFFER_SIZE];
//Open the file
FILE *fp;
// TCP header structure
    struct tcp_segment {
        //unsigned short int src_port;//16 bits
        //unsigned short int dest_port;//16 bits
        unsigned int seq_num;//32 bits
        unsigned int ack_num;//32 bits
        unsigned short int syn;//1 bit flag：Synchronize Sequence Numbers
        unsigned short int fin;//1 bit flag：Acknowledgment
        int data_length; //length of data
        char data[SEGMENT_DATA_SIZE + 1];
    };
void diep(char *s) {
    perror(s);
    exit(1);
}
//read bytes into buffer
int read_bytes(FILE *file, unsigned long long int n, char buffer[]){
     // 移动文件指针到第n个字节
    if (fseek(file, n, SEEK_SET) != 0) {
        perror("Error seeking file");
        fclose(file);
        return -1;
    }
    
    int bytesRead = fread(buffer, sizeof(char), SEGMENT_DATA_SIZE, file);
    if (bytesRead == 0 && feof(file)) {
        // 文件已经读取完毕
        return 0;
    } else if (bytesRead == 0 && ferror(file)) {
        // 发生了读取错误
        perror("Error reading file");
        fclose(file);
        return -1;
    }
    return bytesRead;
}
//read bytes from buffer to segment
int read_bytes_to_segment(char buffer[], struct tcp_segment *segment_to_send, int start_seq, int seq_num_to_send){
    int bytes_read = read_bytes(fp, seq_num_to_send - start_seq, buffer);
    if (bytes_read == -1){
        diep("read_bytes error");
    }
    if (bytes_read == 0){
        //if the file is read completely, break
        return 0;
    }
    segment_to_send->data_length = bytes_read;
    // 清理残余数据
    memset(segment_to_send->data, '\0', sizeof(segment_to_send->data));
    memcpy(segment_to_send->data, buffer, bytes_read);
    return bytes_read;
}

// Global variables for timer
struct timeval last_ack_time;
#define TIMER_INTERVAL_MS 100 // 500ms
// 检查是否到了重传时间
int is_time_to_resend() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);//获取当前时间
    int elapsed_ms = (current_time.tv_sec - last_ack_time.tv_sec) * 1000 + 
                        (current_time.tv_usec - last_ack_time.tv_usec) / 1000;
    return (elapsed_ms >= TIMER_INTERVAL_MS);
}
void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    if (bytesToTransfer == 0) {
        fprintf(stderr, "File is empty\n");
        return;
    }
   
    
    //socket_size = sizeof (si_other);//size of the socket
    socklen_t socket_size = sizeof (si_other);
    fp = fopen(filename, "rb");
    int bytes_read, bytes_sent;
    //check if the file is opened successfully
    if (fp == NULL) {
        diep("fopen");
    }
	/* Determine how many bytes to transfer */
    //create a socket( UDP )
    if ((socket_descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");
    memset((char *) &si_other, 0, sizeof (si_other));//initialize the socket
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);//set the port

    //convert hostname to IP address
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    /* Setting up connection for TCP */
    unsigned int client_isn = 0;
    int ack_num_sent = 0;
    int state = 0;//state of the connection, 0:CLOSED, 1:SYN_SENT, 2:ESTABLISHED
    while (1){
        //header for the request to connect
        //SYN segment header
        struct tcp_segment syn_header;//SYN sender to receiver
        //set random client_isn
        srand(getpid());//设置随机数种子   
        client_isn = random();
        syn_header.seq_num = client_isn;
        syn_header.ack_num = 0;
        syn_header.syn = 1;//set syn flag
        syn_header.fin = 0;
        syn_header.data[0] = '\0';
        // data is null
        //send the SYN segment
        //state: SYN_SENT
        if(sendto(socket_descriptor, &syn_header, sizeof(syn_header), 0, (struct sockaddr *)&si_other, socket_size) == -1){
            diep("sendto");
        }
        else{
            state = 1;
        }
        //receive the SYN-ACK segment
        struct tcp_segment syn_ack_header;//recever to sender
        //if recvfrom is not successful, keep trying
        while(recvfrom(socket_descriptor, &syn_ack_header, sizeof(syn_ack_header), 0, (struct sockaddr *)&si_other, &socket_size) == -1){
            sendto(socket_descriptor, &syn_header, sizeof(syn_header), 0, (struct sockaddr *)&si_other, socket_size);
        }
        //check if the SYN-ACK segment is correct
        if(syn_ack_header.syn == 1 && syn_ack_header.ack_num == syn_header.seq_num + 1){
            //send the ACK segment, the third segment
            struct tcp_segment ack_header;//ACK sender to receiver
            ack_header.seq_num = syn_ack_header.ack_num;//
            ack_header.ack_num = syn_ack_header.seq_num + 1;
            ack_num_sent = ack_header.ack_num;
            ack_header.syn = 0;
            ack_header.fin = 0;
            ack_header.data[0] = '\0';
            if(sendto(socket_descriptor, &ack_header, sizeof(ack_header), 0, (struct sockaddr *)&si_other, socket_size) == -1){
                diep("sendto");
            }
            state = 2;
        }
        else{
            //if the SYN-ACK segment is not correct, exit
            printf("SYN-ACK segment is not correct\n");
            exit(1);
        }
        //state: ESTABLISHED
        if(state == 2)
            break;
    }
    printf("starting to send data");
	/* Send data and receive acknowledgements on s*/
    //client_isn: syn message
    int start_seq = client_isn + 2;//starting seq num
    struct tcp_segment segment_to_send;
    int seq_num_to_send = start_seq;//seq num of the segment to be sent
    struct tcp_segment ack_segment;//ack segment
    int seq_num_sent = start_seq;//seq num of last segment sent which was acked
    int timer_flag = 0;//0: timer is not set, 1: timer is set
    int num_of_duplicate_ack = 0;  
    while (1){  
        if(seq_num_to_send - start_seq<= bytesToTransfer){
            segment_to_send.seq_num = seq_num_to_send;
            segment_to_send.ack_num = ack_num_sent + 1;
            segment_to_send.syn = 0;
            segment_to_send.fin = 0;
            //read bytes to segment
            bytes_read = read_bytes_to_segment(buffer, &segment_to_send, start_seq, seq_num_to_send);
            if(bytes_read == 0){
                break;//if the file is read completely, break
            }
            if(timer_flag==0){
                //start the timer if it is not set
                gettimeofday(&last_ack_time, NULL);
                timer_flag = 1;
            }
            bytes_sent = sendto(socket_descriptor, &segment_to_send, sizeof(segment_to_send), 0, (struct sockaddr *)&si_other, socket_size);
            if (bytes_sent == -1) {
                //if sendto is not successful, exit
                diep("sendto");
            }
            else{
                //sent successfully
                //start the timer
                gettimeofday(&last_ack_time, NULL);
                timer_flag = 1;
            }
            seq_num_to_send += SEGMENT_DATA_SIZE;//next segment
        }//end if seq_num_to_send
        //if timeout, resend the segment
        if(is_time_to_resend()){
            //send the segment with seq_num_sent(smallest seq num of not yet acked segment)
            segment_to_send.seq_num = seq_num_sent;
            segment_to_send.ack_num = ack_num_sent + 1;
            segment_to_send.syn = 0;
            segment_to_send.fin = 0;
            //read bytes to segment
            bytes_read = read_bytes_to_segment(buffer, &segment_to_send, start_seq, seq_num_sent);
            if(bytes_read == 0){
                break;//if the file is read completely, break
            }
            bytes_sent = sendto(socket_descriptor, &segment_to_send, sizeof(segment_to_send), 0, (struct sockaddr *)&si_other, socket_size);
            if (bytes_sent == -1) {
                //if sendto is not successful, exit
                diep("sendto");
            }
            else{
                //sent successfully
                //start the timer
                gettimeofday(&last_ack_time, NULL);
                timer_flag = 1;
            }
        }//end if timeout
        //if all segments are sent, close the connection
        if (seq_num_sent - start_seq > bytesToTransfer){
            break;
        }
        //if receive ack msg, send the next segment
        if (recvfrom(socket_descriptor, &ack_segment, sizeof(ack_segment), 0, (struct sockaddr *)&si_other, &socket_size) == -1){
            diep("receive ack error");
        }
        else{
            //receive the ack segment
            ack_num_sent = ack_segment.seq_num;
            //if the ack segment is correct, update the seq_num_sent
            if(ack_segment.ack_num > seq_num_sent){
                seq_num_sent = ack_segment.ack_num;
                //there are currently any not-yet-acknowledged segments 
                // start timer
                if(seq_num_sent < seq_num_to_send){
                    gettimeofday(&last_ack_time, NULL);
                    timer_flag = 1;
                }
            }
            else{
                //duplicate ack
                num_of_duplicate_ack++;
                if(num_of_duplicate_ack == 3){
                    //fast retransmit
                    //resend the segment with seq_num_sent
                    segment_to_send.seq_num = seq_num_sent;
                    segment_to_send.ack_num = ack_num_sent + 1;
                    segment_to_send.syn = 0;
                    segment_to_send.fin = 0;
                    //read bytes to segment
                    bytes_read = read_bytes_to_segment(buffer, &segment_to_send, start_seq, seq_num_sent);
                    if(bytes_read == 0){
                        break;//if the file is read completely, break
                    }
                    bytes_sent = sendto(socket_descriptor, &segment_to_send, sizeof(segment_to_send), 0, (struct sockaddr *)&si_other, socket_size);
                    if (bytes_sent == -1) {
                        //if sendto is not successful, exit
                        diep("sendto");
                    }
                    else{
                        //sent successfully
                        //start the timer
                        num_of_duplicate_ack = 0;
                        gettimeofday(&last_ack_time, NULL);
                        timer_flag = 1;
                    }
                }
            }
        }
        
        
    }    
    //close the connection
    //fin_state: 0: FIN_WAIT_1, 1: FIN_WAIT_2, 2: TIME_WAIT
    //int fin_state = 0;
    //send the fin segment
    struct tcp_segment fin_header;
    fin_header.seq_num = seq_num_to_send;
    fin_header.ack_num = ack_segment.seq_num + 1;    
    fin_header.syn = 0;
    fin_header.fin = 1;
    fin_header.data[0] = '\0';
    while(1){
        //fin_state = 0://FIN_WAIT_0: fin sent, waiting for ack
        //receive the ack segment
        struct tcp_segment fin_ack_header;
        if(sendto(socket_descriptor, &fin_header, sizeof(fin_header), 0, (struct sockaddr *)&si_other, socket_size) == -1){
                diep("sendto");
        }
        //receive the ack segment
        if (recvfrom(socket_descriptor, &fin_ack_header, sizeof(fin_ack_header), 0, (struct sockaddr *)&si_other, &socket_size) == -1){
            diep("receive ack error");
        }
        //if the ack segment is correct, fin_state = FIN_WAIT_2
        if (fin_ack_header.ack_num == fin_header.seq_num + 1){
            //fin_state = 1;
            break;
        }
    }
    while (1)
    {
       //fin_state = 1, FIN_WAIT_1, waiting for fin from server
        //receive the fin segment
        struct tcp_segment fin_server;
        if (recvfrom(socket_descriptor, &fin_server, sizeof(fin_server), 0, (struct sockaddr *)&si_other, &socket_size) == -1){
            diep("receive fin error");
        }
        //if the fin segment is correct, fin_state = TIME_WAIT
        if (fin_server.fin == 1){
            //send the ack segment,start timer
            struct tcp_segment fin_server_ack;
            fin_server_ack.seq_num = fin_server.ack_num;
            fin_server_ack.ack_num = fin_server.seq_num + 1;
            fin_server_ack.syn = 0;
            fin_server_ack.fin = 0;
            fin_server_ack.data[0] = '\0';
            if(sendto(socket_descriptor, &fin_server_ack, sizeof(fin_server_ack), 0, (struct sockaddr *)&si_other, socket_size) == -1){
                diep("sendto");
            }
            //fin_state = 2;
            break;
        }

    }
    //fin_state = 2: TIME_WAIT, wait for 30s
    // if(fin_state == 2){
    //     sleep(30);
    // }
    //printf("Finished sending the file.\n");
    fclose(fp);
    printf("Closing the socket\n");
    close(socket_descriptor);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}


