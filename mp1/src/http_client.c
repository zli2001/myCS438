/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd;  
	char buf[1024];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	struct Info {
		char service[12];
		char address[1024];
		char port_number[128];
		char file_path[1024];
	}info1;

	memset(&info1, 0, sizeof(info1));
	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	// Hostname format http://hostname[:port]/path/to/file

	char *outer_saveptr = NULL;
	char *inner_saveptr = NULL;
	char *ptr = strtok_r(argv[1], "/",&outer_saveptr);

	int i = 0;
	char temp[1024];

	while(ptr != NULL)
	{
		// printf("'%s'\n", ptr);
		if (i == 0){
			strcpy(info1.service, ptr);
		}
		else if (i == 1){
			
			strcpy(temp, ptr);

			char *inner_ptr = strtok_r(temp, ":", &inner_saveptr);
			int z = 0;
			while(inner_ptr != NULL){
				if (z == 0) {
					strcpy(info1.address, inner_ptr);
				}
				else {
					strcpy(info1.port_number, inner_ptr);
				}
				z++;
				inner_ptr = strtok_r(NULL, ":", &inner_saveptr);
			}
			if(strlen(info1.port_number) == 0) { // set port to 80 if no port is given
				strcpy(info1.port_number, "80");
			}
		}
		else{
			// printf("'%s Path'\n", ptr);
			strcat(info1.file_path, "/");
			strcat(info1.file_path, ptr); 
		}
		i++;

		ptr = strtok_r(NULL, "/",&outer_saveptr);
	}
	// printf("'%s Service'\n", info1.service);
	// printf("'%s Address. %s Port.'\n", info1.address, info1.port_number);
	// printf("'%s File Path'\n", info1.file_path);
	// printf("'%ld File Path Length'\n", strlen(info1.file_path));



	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// printf("client address %s\n", argv[1]);

	if ((rv = getaddrinfo(info1.address, info1.port_number, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// GET /test.txt HTTP/1.1
	// User-Agent: Wget/1.12 (linux-gnu)
	// Host: localhost:3490
	// Connection: Keep-Alive

	char req_message[1024] = "GET ";
	strcat(req_message,info1.file_path);
	strcat(req_message, " HTTP/1.1\r\n");
	strcat(req_message, "User-Agent: Wget/1.12 (linux-gnu)\r\n");
	strcat(req_message, "Host: ");
	strcat(req_message, info1.address);
	strcat(req_message, ":");
	strcat(req_message, info1.port_number);
	strcat(req_message, "\r\n");
	strcat(req_message, "Connection: Keep-Alive\r\n\r\n");

	// request
	if (send(sockfd, req_message, sizeof(req_message), 0) == -1)
				perror("send");

	// recieve file and write it to out
	// FILE *fp = fopen("output", "w");
	// while(1) {
	// 	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	// 		perror("recv");
	// 		exit(1);
	// 	}
	// 	if(numbytes <= 0) {
	// 		break;
	// 	}
	// 	// fwrite(buf, sizeof(char), 100-1,fp);
	// }

	// fclose(fp);
	// if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	// 		perror("recv");
	// 		exit(1);
	// 	}
	// printf("buffer:%s\n",buf);
	// buf[numbytes] = '\0';
	//char *content = buf;
	//char HTTP_code[4];
	//char HTTP_header[100];
	//int cnt = 0;
	//read first line 
	FILE *fp = fopen("output", "wb");
	int received_bytes;
	if (fp == NULL) {
        perror("Error while opening output file");
    }
	//http_head
	int flag = 0;
	while ((received_bytes = recv(sockfd, buf, 1024, 0)) > 0) {
		if(flag == 0){
			//print and write header
			char *HTTP_header = strtok(buf, "\r\n\r\n");
			int header_bytes = strlen(HTTP_header) + 4;
			printf("%s\n",HTTP_header);
			if (strstr(HTTP_header, "200") == NULL){
				break;
			}
			fwrite(buf + header_bytes, 1, received_bytes-header_bytes, fp);
			flag=1;
			continue;
		}
		if (received_bytes == 0) {
            printf("No data received\n");
            break;
        }
    	fwrite(buf, 1, received_bytes, fp);
    }
	fclose(fp);


	// char *HTTP_header = strtok(buf, "\r\n\r\n");
	// printf("%s\n",HTTP_header);
	
	// int header_bytes = strlen(HTTP_header);
	// printf("%d\n",header_bytes);
	// //HTTP_header[cnt]='\0';//null-terminate
	// //printf("%s\n",HTTP_header);
	// //printf(*HTTP_header++);
	// if (strstr(HTTP_header, "200") == NULL) {
    // 	printf("not found");
    // }
	// }
	
	close(sockfd);

	return 0;
}

