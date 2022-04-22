/* 
   A simple server in the internet domain using TCP
   Usage:./server port (E.g. ./server 10000 )
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <sys/stat.h>
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define HEADER_FORMAT "HTTP/1.1 %d %s\nContent-Length: %ld\nContent-Type: %s\n\n"
#define NOT_FOUND_404 "<h1>404 Not Found</h1>\n"
#define SERVER_ERROR_500 "<h1>500 Server Error</h1>\n"

void error(char *msg)
{
    perror(msg);
    exit(1);
}

// server의 ip addr과 port를 소켓에 할당
int bindServer(int s_sock, int port){
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET; // type : IPV4
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // IP addr
	server_addr.sin_port = htons(port); // port
	// socket ip, port 할당을 위해 bind()를 통해 socket file descriptor를 넘겨줌  
	// s_sock에 server_addr를 할당.
	return bind(s_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
}

// 알맞은 파일 타입을 넣어줌.  (ex: img.jpeg >> jpeg, img.gif >> gif)
void file_type(char *type, char *uri){
	char *ext = strrchr(uri, '.'); // ext=extension

	if (!strcmp(ext, ".html")){ 
		strcpy(type, "text/html");
	}
	else if (!strcmp(ext, ".gif")){
		strcpy(type, "image/gif");
	}
	else if (!strcmp(ext, ".jpeg")){
		strcpy(type, "image/jpeg");
	}
	else if (!strcmp(ext, ".mp3")){
		strcpy(type, "text/mpeg3");
	}
	else if (!strcmp(ext, ".pdf")){
		strcpy(type, "application/pdf");
	}
}

// 헤더 내용, 현 상태를 포인터에 넣어줌.
void HTTP_header(char *header, int status, long len, char *type){
	char status_text[40]; //header size 40
	switch (status){
		case 200:
			strcpy(status_text, "200 OK");
			break;
		case 404:
			strcpy(status_text, "404 Not Found");
			break;
		case 500:
		default:
			strcpy(status_text, "500 Server Error");
	}
	// 모은 정보를 header에 작성
	sprintf(header, HEADER_FORMAT, status, status_text, len, type);
}


// 404 not found error
void error_404(int c_sock){
	char header[BUFFER_SIZE];
   
   	// HTTP_header status가 404일 때, type=html
	HTTP_header(header, 404, sizeof(NOT_FOUND_404), "text/html"); 

	write(c_sock, header, strlen(header));
	write(c_sock, NOT_FOUND_404, sizeof(NOT_FOUND_404));
}

// server error(500) 
void error_500(int c_sock){
	char header[BUFFER_SIZE];

   	//HTTP_header status가 500일 때, type=html
	HTTP_header(header, 500, sizeof(SERVER_ERROR_500), "text/html"); 

	write(c_sock, header, strlen(header));
	write(c_sock, SERVER_ERROR_500, sizeof(SERVER_ERROR_500));
}

//파일 불러오기가 성공하면 파일의 내용을 성공적으로 전송한다.(200)
//파일 전송을 실패하면 error 함수들을 호출한다.
void http_handler(int c_sock){
	char header[BUFFER_SIZE];
	char buf[BUFFER_SIZE];

   	//서버 소켓 읽기 실패
	if (read(c_sock, buf, BUFFER_SIZE) == -1) {
		perror("failed to read request.\n");
		error_500(c_sock);
		return;
	}

	char *method = strtok(buf, " "); //method 포인터
	char *uri = strtok(NULL, " "); // URI

   	//uri method 인식 실패
  	// method uri가 null일 경우
	if (method == NULL || uri == NULL){
		perror("failed to identify URI method.\n");
		error_500(c_sock);
		return;
	}
	printf("%s / HTTP/1.1\nURI=%s\n", method, uri); //print request method, uri


	char connect_uri[BUFFER_SIZE];
	char *loc_uri; //local uri
	struct stat st; //파일정보저장 

	strcpy(connect_uri, uri); // conn_uri에 uri복사
	//localhost connecting : connect페이지를 보여준다.
	if (!strcmp(connect_uri, "/")){
		strcpy(connect_uri, "/connect.html");
	}
	loc_uri = connect_uri + 1;

   	//매칭되는 파일이 없을 경우, 404 Not Found
	if (stat(loc_uri, &st) == -1){
		perror("\n>>>404 Not Found\n>>>Failed to found file.\n\n");
		error_404(c_sock);
		return;
	}

	int fd = open(loc_uri, O_RDONLY); //읽기 전용으로 열기
   	//파일 오픈에 실패했을 경우, Server Error
	if (fd == -1){
		perror("\n>>>500 Server Error\n>>>Failed to open file\n\n");
		error_500(c_sock);
		return;
	}

	int len = st.st_size; //st_size=file size.  
	char type[40];

	file_type(type, loc_uri);
	HTTP_header(header, 200, len, type);
	write(c_sock, header, strlen(header));

	int tmp;
   	//file content가 buffer보다 클 경우, buffer size만큼 읽음.
	while ((tmp = read(fd, buf, BUFFER_SIZE)) > 0){
	   write(c_sock, buf, tmp);
   	}
}

int main(int argc, char **argv){
	int port, pid, s_sock, c_sock;

	struct sockaddr_in c_addr; //socket_in= socket addr 틀 형성하는 구조체
	socklen_t c_addr_len; // client_address 주소  
	//명령어 문자열 2미만일 경우
	if (argc < 2){
		printf("Usage: \n\t%s {port}: runs HTTP server.\n", argv[0]); // ./server 문자열
	}
	//port#. ex)./server 8080 > port#:8080
	port = atoi(argv[1]);
	//argv[1] contains the first argument so atoi(argv[1]) will convert the first argument to an int.

	printf("=======Host: 127.0.0.1: %d=======\n", port); // listen port#

   	//TCP연결 socket 생성, stream방식
	s_sock = socket(AF_INET, SOCK_STREAM, 0); 
	if (s_sock == -1){
		perror("ERROR: socket error\n");
	}
   	// 소켓, 서버 주소 bind
	if (bindServer(s_sock, port) == -1){   
		perror("ERROR: bind error\n");
	}
   	//연결 대기열 5개 생성, listen함수 호출
	if (listen(s_sock, 5) == -1){
		perror("ERROR: listen error\n");
	}
	// signal handler
	signal(SIGCHLD, SIG_IGN);
   	//client request를 받고 response
	while(1){
		printf("\n>>>Waiting....\n");
		c_sock = accept(s_sock, (struct sockaddr *)&c_addr, &c_addr_len);
      // client 요청이 오면 connect accept
		if (c_sock == -1){
		    perror("ERROR: accept error.\n");
		}
		pid = fork();
		if (pid == 0){
		    close(s_sock);

		    http_handler(c_sock);  
		    close(c_sock);
		    exit(0);
		}
		if (pid != 0){
			close(c_sock);
		}
		if (pid == -1){
			perror("ERROR: fork error.\n");
		}	
	}
}



