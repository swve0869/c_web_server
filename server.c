/*
	Swami Velamala 
	C socket server
*/
#include <stdio.h>
#include <string.h>	//strlen
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>	//inet_addr
#include <unistd.h>	//write
#include <string.h>
#include <stdlib.h>

#define BUFSIZE 1024


int fsize(FILE* fp){
	fseek(fp, 0, SEEK_END); // seek to end of file
	int size = ftell(fp); // get current file pointer
	fseek(fp, 0, SEEK_SET); // seek back to beginning of fi
	return size;
}

// function to parse filetype and generate the http filetype
int gen_filename_ext(const char* filename, char* filetype) {  
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return -1;

	if(strcmp(dot,".htm") == 0){
		strcat(filetype,"text/html");
		return 0;
	}
	if(strcmp(dot,".html") == 0){
		strcat(filetype,"text/html");
		return 0;
	}
	if(strcmp(dot,".txt") == 0){
		strcat(filetype,"text/plain");
		return 0;
	}
	if(strcmp(dot,".png") == 0){
		strcat(filetype,"image/png");
		return 0;
	}
	if(strcmp(dot,".gif") == 0){
		strcat(filetype,"image/gif");
		return 0;
	}
	if(strcmp(dot,".jpg") == 0){
		strcat(filetype,"image/jpg");
		return 0;
	}
	if(strcmp(dot,".css") == 0){
		strcat(filetype,"text/css");
		return 0;
	}
	if(strcmp(dot,".js") == 0){
		strcat(filetype,"application/javascript");
		return 0;
	}
	if(strcmp(dot,".ico") == 0){
		strcat(filetype,"image/x-icon  ");
		return 0;
	}

	return -1;
}

int malformed(char* request){
	char c;
	int space_count = 0;
	for(int i = 0; i < strlen(request); i++){
		c = request[i];
		if(c == '\r'){
			break;
		}
		if(c == ' '){
			space_count ++;
			if(space_count > 2){return -1;}
		}
	}
	return 0;
}


int create_header(char* request,char* header,char* filepath, char* index, int *bytes, int *alive){
	//printf("%s\n",request);
	if(malformed(request) == -1){
		strcat(header,"400 Bad Request\r\n\r\n");
		return 400;
	}

	char *tok1 , *tok2;
	char filetype[15]  = {'\0'};
	char filesize[100] = {'\0'};
    char reqtype[100]  = {'\0'};
	char path[1000]    = {'\0'};
	char version[20]   = {'\0'};
	char keepalive[15] = {'\0'};
	strcat(filepath,"www");
    strcat(reqtype,strtok(request," ")); 
    strcat(path,strtok(NULL," ")); 
	strcat(version,strtok(NULL,"\r\n")); 
	
	printf("|%s|%s|%s|\n",reqtype,path,version);

	// parse rest of header to see for connection keep alive
	while(1){
		tok1 = strtok(NULL,"\r\n");
		char temp[100] = {'\0'};
		if(tok1 == NULL){ 
			break;
		}else{
			strncpy(temp,tok1,11);
		}
		if(strcmp(temp,"Connection:") == 0){
			if(strcmp(tok1+12,"keep-alive") == 0) *alive = 1;
			break;
		}else{
			*alive = 0;
		}		
	}

	// check that request type is GET if not create error response 
	if(strcmp(reqtype,"GET") != 0){
        strcat(header,"405 Method Not Allowed\r\n\r\n");
		return 405;
	} 
	
	// check that HTTP version is correct
	if(strcmp(version,"HTTP/1.0") != 0 && strcmp(version,"HTTP/1.1") != 0){
		strcat(header,"505 HTTP Version Not Supported\r\n\r\n");
		return 505;
	}

	// create beginning of header "HTTP version"
	strcat(header, version); 
	strcat(header," ");

    //check if the GET request is generic
    if(strcmp(path,"/") == 0 || strcmp(path,"/inside/") == 0 ||
		strcmp(path,"/index.htm") == 0){
		strcat(filepath,index);
    }else{
		strcat(filepath,path);
	}

	// check if file exists
	if (access(filepath, F_OK) != 0) {
		strcat(header,"404 Not Found\r\n\r\n");
		return 404;
	} 
	// check read write priviliges
	if (access(filepath,R_OK) != 0){
		strcat(header,"403 Forbidden\r\n\r\n");
		return 403;
	}
	
	// get th file content type
	if(gen_filename_ext(filepath,filetype) != 0){
		strcat(header,"400 Bad Request\r\n\r\n");
		return 400;
	}

	FILE * fp = fopen(filepath,"r");
	*bytes = fsize(fp);
	sprintf(filesize,"%d",*bytes);
	fclose(fp);

	// assemble response
	strcat(header, "200 OK"); strcat(header,"\n");
	strcat(header,"Content-Type: "); 
	strcat(header,filetype); strcat(header,"\n");
	strcat(header,"Content-Length: "); 
	strcat(header,filesize); strcat(header,"\n");
	if(*alive == 1){
		strcat(header,"Connection: keep-alive"); 
	}
	else{
		strcat(header,"Connection: close"); 
	}
	strcat(header,"\r\n\r\n");
    return 0;
}

//--------------------------------------------------------------------------
int main(int argc , char *argv[])
{
	if(argc < 2){
		printf("not enough args\n");
		return -1;
	}

	int port = atoi(argv[1]);
	if(port <= 1024){
		printf("invalid port #\n");
		return -1;
	}
	char * index =  malloc(20);

	if (access("www/index.html", F_OK) == 0) {
		strcat(index,"/index.html");
	}
	if (access("www/index.htm", F_OK) == 0) {
		strcat(index,"/index.htm");
	}

	int total_procs = 0;
	int socket_desc , client_sock , c , read_size;
	struct sockaddr_in server , client;
	int parentid = getpid();

	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket");
	}
	puts("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);
    int a = 1;
	setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(int));

	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	puts("bind done");
	
	//Listen
	listen(socket_desc , 3);
	
	//wait for and Accept an incoming connection
	while(1){
		puts("Waiting for incoming connections...");
		c = sizeof(struct sockaddr_in);
		//accept connection from an incoming client
		client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);

		if (client_sock < 0)
		{
			perror("accept failed");
			return 1;
		}

		// fork process here to service tcp connection
		fork();  
		if(getpid() == parentid){
			continue;
		}
		printf("Connection accepted pid:%d \n" , getpid());

		struct timeval timeout;      
		timeout.tv_usec = 0;
		int alive = 0;

		while(1){
			//Receive a message from client
			int filebytes;
			int bytes_sent = -1;
			int header_length;
			char * header;    header   = (char *) malloc(300);
			char * response;  response = (char *) malloc(1024);
			char * filepath;  filepath = (char *) malloc(1024);

			char request[10000];
			read_size = recv(client_sock ,request , 10000 , 0);
		
			if(read_size == 0)
			{
				printf("Client disconnected pid:%d \n" , getpid());
				close(client_sock);
				break;
			}

			if(read_size == -1)
			{
				printf("timeout Client disconnected pid:%d \n" , getpid());
				close(client_sock);
				return 0;
			}

			int request_status = create_header(request,header,filepath,index,&filebytes,&alive);
			printf("request served on : %d \n", getpid());

			if(alive == 1){
				timeout.tv_sec = 10;
			}else{
				timeout.tv_sec = 0;
			}

			if (setsockopt (client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof timeout) < 0){
        			printf("setsockopt failed\n");
			}

			if(request_status == 0){
				FILE * fp = fopen(filepath,"r");
				while(bytes_sent < filebytes){
					bzero(response,BUFSIZE);
			
					if(bytes_sent == -1){
						strcat(response,header);
						fread(response+strlen(header),1024-strlen(header),1,fp);
						bytes_sent += 1024-strlen(header);
					}
					else{
						fread(response,1024,1,fp);
						bytes_sent += 1024;
					}

					int bytes_sent = send(client_sock,response,BUFSIZE,0);
					if(bytes_sent != BUFSIZE){
						send(client_sock,response+bytes_sent, BUFSIZE - bytes_sent,0);
					}

				}
			//if packet request fails return
			}else{
				printf("failed: error %d \n", request_status);
				send(client_sock,header,strlen(header),0);	
			}

			free(header);
			free(response);
			free(filepath);
			bzero(request,10000);

		}
	}
	
	return 0;
}
