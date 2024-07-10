/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

// #define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold



void sigchld_handler()
{
	
	while(waitpid(-1, NULL, WNOHANG) > 0);
	//wait for all terminated child process  
}

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
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	int numbytes;
	char buf[1024]; // change to max
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	if (argc != 2) {
	    fprintf(stderr,"usage: server port\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	printf("Port # %s\n", argv[1]);

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener

			if ((numbytes = recv(new_fd, buf, 1024, 0)) == -1) {
	   			perror("recv");
	    		exit(1);
			}
			//printf("Messsage Recieved %s", buf);		

			char *ptr = strtok(buf, " ");
			int i = 0;
			char file_path[1024];
			while(ptr != NULL) {
				// printf("%s\n", ptr);
				if (i == 1) {
					strcpy(file_path,ptr);
					break;
				}	
				i ++;
				ptr = strtok(NULL, " ");
			}

			char cwd[1024];
    
			// abs path of cwd
			if (getcwd(cwd, sizeof(cwd)) == NULL) {
				perror("cwd error");
			} 
			if(strstr(file_path,cwd) == NULL){
				strcat(cwd,file_path);
				strcpy(file_path,cwd);
			}
			
			printf("File Path Recieved %s\n", file_path);
			//delete the first '/' in the file path
			// if(file_path[0]=='/'){
			// 	 memmove(file_path, file_path + 1, strlen(file_path));
			// }
			
			//printf("buffer %s", buf);
			
        
			//read and send the file 
			FILE *fp = fopen(file_path, "rb");
			char *ptr_buf = buf;
			//sprintf(ptr_buf,"HTTP/1.1 200 OK\r\n\n");
			//ptr_buf += strlen("HTTP/1.1 200 OK\r\n\n");
	

			if (fp == NULL){
				printf("HTTP/1.1 404 Not Found\r\n\r\n"); // Replace with http error code
				int code_bytes = strlen("HTTP/1.1 404 Not Found\r\n\r\n");
				sprintf(ptr_buf,"HTTP/1.1 404 Not Found\r\n\r\n");
				//ptr_buf += code_bytes;
				if (send(new_fd, buf, code_bytes, 0) == -1)
						perror("error");
				exit(0);
			}

			else {
				//printf("buffer %s", buf);
				sprintf(buf,"HTTP/1.1 200 OK\r\n\r\n");//http header 19 bytes
				int header_bytes = strlen("HTTP/1.1 200 OK\r\n\r\n");
				
				//ptr_buf += code_bytes;
				int bytes_read;
				int file_bytes = 0;
				//read the file
				FILE *output_file = fopen("out.txt", "wb");
				if (output_file == NULL) {
					perror("Error opening output file");
					return 1;
				}
				//fwrite(buf, 1, code_bytes, output_file);
				if (send(new_fd, buf, header_bytes, 0) == -1) {
					perror("send");
					fclose(fp);
					return 1;
				}

				while ((bytes_read = fread(buf, 1, 1024, fp)) > 0) {
					if (send(new_fd, buf, bytes_read, 0) == -1) {
						perror("send");
						fclose(fp);
						fclose(output_file);
						return 1;
					}
					file_bytes += bytes_read;
					//buf += bytes_read;
					fwrite(buf, 1, bytes_read, output_file);
				}
				printf("%d",file_bytes);
				// FILE *output_file = fopen("out.txt", "wb");
				// if (output_file == NULL) {
				// 	perror("Error opening output file");
				// 	return 1;
				// }
				// fwrite(buf, 1, code_bytes + file_bytes, output_file);
				
				// while (!feof(fp)) {
				// 	int bytes_read = fread(ptr_buf, 1 ,1024, fp);
				// 	//write send package
				// 		FILE *output_file;
				// 		size_t bytes_r;
				// 	    output_file = fopen("output", "wb");
				// 		if (output_file == NULL) {
				// 			perror("Error opening output file");
				// 		}
				// 		while ((bytes_r = fread(buffer, 1, sizeof(buf), fp)) > 0) 
				// 		{
        		// 			fwrite(buffer, 1, bytes_r, output_file);
    			// 		}
					
	
				// }
				
			}
			fclose(fp);
			
			// if (send(new_fd, "Hello, world!", 13, 0) == -1)
			// 	perror("send");
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

