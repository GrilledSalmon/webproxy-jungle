/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize,char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiarg, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);



int main(int argc, char **argv) {
  int listenfd, connfd; // fd 둘 다 int형이네??
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) { // 입력값(port)을 올바르게 작성하지 않았다면
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 열어줄 port를 인자로 받아 listenfd 생성
  listenfd = Open_listenfd(argv[1]);
  
  while (1) {
    clientlen = sizeof(clientaddr);

    // client의 연결요청이 들어오는지 listen  
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    // clientaddr에 있는 client의 정보 바탕으로 hostname과 port에 client의 주소와 포트 정보 저장
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Request from (%s:%s) \n", hostname, port);
    doit(connfd);   // 트랜잭션 수행
    Close(connfd);  // 트랜잭션이 수행된 후 자신쪽의 연결 끝(소켓) 을 닫는다.

  }
  // 트랜잭션 수행 함수
}



// fd로 connfd가 들어감
void doit(int fd)
{
  int is_static;
  struct stat sbuf; // 파일의 정보가 담기는 buf
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];

  // rio_readlineb를 위해 rio_t 타입(구조체)의 읽기 버퍼를 선언
  rio_t rio;
  //rio_t 구조체를 초기화 해준다.
  Rio_readinitb(&rio, fd);
  // Rio_readlineb를 통해 요청 라인을 읽어들이고, 분석한다.
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers : \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);

  // TINY는 GET method만 지원하기에 클라이언트가 다른 메소드를 요청하면
  // 에러 메시지를 보내고, main routin으로 돌아온다.

  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")){
    clienterror(fd, method, "501", "Not implemented",
                "GET요청만 받을 수 있습니다.");
    return;
  }
  // get method 라면 읽어들이고, 다른 요청 헤더들 을 무시한다.
  read_requesthdrs(&rio);

  printf("-------------여기까지가 Client Request--------------\n");


  // parse uri form GET request
  // URI를 파일이름과 비어 있을 수도 있는 CGI 인자 스트링으로 분석하고, 요청이 정적 또는 동적 컨텐츠를 위한 것인 지 나타내는
  // 플래그를 설정한다.
  is_static = parse_uri(uri, filename, cgiargs);

  if (stat(filename, &sbuf) < 0){ // filename 경로의 파일을 읽어서 sbuf에 내용 저장
    clienterror(fd, filename, "404", "Not Found", "파일을 찾을 수 없습니다.");
    return;
  }

  if (is_static){// 정적 컨텐츠
    //동적 컨텐츠이고, 이 파일이 보통파일인지, 읽기 권한을 가지고 있는지 검증
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "파일을 읽을 수 없습니다.");
      return;
    }
    //그렇다면 정적 컨텐츠를 클라이언트에게 제공
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else{ //동적 컨텐츠
    //실행 가능한 파일인지 검증
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "파일을 읽을 수 없습니다");
      return;
    }
    //그렇다면 동적 컨텐츠를 클라이언트에게 제공
    serve_dynamic(fd, filename, cgiargs, method);
  } 
  
}



/* 명백한 오류에 대해 클라이언트에 보고하는 함수.
  HTTP응답을 응답 라인에 적절한 상태 코드와 
  상태 메시지와 함께 클라이언트에 보낸다.*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];
  /* BUILD the HTTP respons body
    브라우저 사용자에게 에러를 설명하는 응답 본체에 HTMLeh 함께 보낸다.
    HTML 응답은 본체에서 컨텐츠의 크기와 타입을 나타내야 하기에, HTML 컨텐츠를 한 개의 스트링으로 만든다.
    이는 sprintf를 통해 body는 인자에 스택되어 하나의 긴 스트링에 저장된다.*/

  sprintf(body, "<html><title>ERROR</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n",body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>윤우's 웹 서버</em>\r\n", body);
  //body 배열에 html을 넣어준다.

  //print the HTTP response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);

  //Rio_writen 그림 10.3
  //굳이 보내고 쌓고 보내고 쌓고가 아니라 위에 body처럼 쭉 쌓아서 한번에 보내도 되지 않을까?
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: test/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  //buf에 넣고 보내고 넣고 보내고

  //sprintf에 쌓아놓은 길쭉한 배열을 (body, buf)를 보내준다.
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* tiny는 요청 헤더 내의 어떤 정보도 사용하지 않는다.
 * 단순히 이들을 읽고 무시한다.
 */
void read_requuesthdrs(rio_t *rp){
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);
  printf("----------------%s", buf);
  /* strcmp 두 문자열을 비교하는 함수 */
  /* 헤더의 마지막 줄은 비어있기에 \r\n만 buf에 담겨 있다면 while문을 탈출한다. */
  while (strcmp(buf, "\r\n")){
    //rio_readlineb는 \n을 만날때 멈춘다.
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    //멈춘 지점 까지 출력하고 다시 while
  }
  return;
}



/* Parse URI from GET request
 * URI를 파일 이름과 비어 있을 수 도 있는 CGI 인자 스트링으로 분석하고
 * 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내는 플래그를 설정한다.*/
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;
  //strstr 으로 cgi-bin이 들어있는지 확인하고 양수값을 리턴하면 dynamic content를 
  //요구하는 것이기에 조건문을 탈출.
  //static content
  if (!strstr(uri, "cgi-bin")){ // uri 안에 cgi-bin이 없으면
    strcpy(cgiargs, "");
    strcpy(filename,".");
    strcat(filename, uri);
    // 결과 cgiargs="" 공백 문자열, filename="./~~  or ./home.html
    // uri 문자열 끝이 /일 경우 허전하지 말라고 home.html을 filename에 붙혀준다.

    if(uri[strlen(uri)-1]=='/'){
      strcat(filename, "home.html");
    }
    return 1;
  } 
  else { // uri에 cgi-bin이 있는 경우 인자 값 parsing
    //index 함수는 문자열에서 특정 문자의 위치를 반환한다.
    ptr=index(uri,'?');
    //?가 존재한다면
    if(ptr){
      strcpy(cgiargs,ptr+1);
      *ptr='\0';
    }else{
      strcpy(cgiargs,"");
    }
    strcpy(filename,".");
    strcat(filename,uri);
    return 0;
  }
}



  /* get_filetype - Derive file type from filename
   * strstr 두번째 인자가 첫번째 인자에 들어있는지 확인
   * filename을 보고 어떤 타입인지 확인해 filetype에 넣어줌
   */
void get_filetype(char *filename, char *filetype){
  if(strstr(filename, ".html")){
    strcpy(filetype, "text/html");
  }else if(strstr(filename, ".gif")){
    strcpy(filetype, "image/gif");
  }else if(strstr(filename, ".png")){
    strcpy(filetype,"image/png");
  }else if(strstr(filename,".jpg")){
    strcpy(filetype,"image/jpeg");
  }else if(strstr(filename, ".mp4")){
    strcpy(filetype, "video/mp4");
  }else if(strstr(filename, ".mpeg")){
    strcpy(filetype, "video/mpeg");
  }else if(strstr(filename, ".MOV")){
    strcpy(filetype, "video/MOV");
  }else{
    strcpy(filetype, "text/plain");
  }
}



/*
1. 주석 처리된 첫번째 경우 Mmap함수를 이용해 바로 메모리를 할당하며
  srcfd의 파일 값을 배정한다.
2. malloc의 경우 filesize 만큼의 가상메모리를 할당한 후, Rio_readn으로 할당된 
  가상 메모리 공간의 시작점인 fbuf를 기준으로 srcfd파일을 읽어 복사해넣는다.
3. 양쪽 모두 생성한 파일 식별자 번호인 srcfd를 Close()해주고
4. Rio_writen 함수(시스템 콜) 을 통해 클라이언트에게 전송한다.
5. 마지막으로 Mmap은 Munmap, malloc은 free로 할당된 가상 메모리를 해제해준다. 
*/
void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF], *fbuf;
  /* Send response headers to client */
  get_filetype(filename, filetype); // filename을 보고 파일이 어떤 타입인지 filetype에 저장
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  
  /* writen = client 쪽에 */
  Rio_writen(fd, buf, strlen(buf)); // buf 내용(response 개요)을 fd에 작성
  /* 서버쪽에 출력 */
  printf("Response headers:\n");
  printf("%s", buf); // response 개요를 출력
  if (!strcasecmp(method, "HEAD")){ // HEAD라는 method가 있나보넹
    return;
  }
  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // read only
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  srcp = (char*)Malloc(filesize);
  Rio_readn(srcfd, srcp, filesize); // 파일(srcfd)의 내용을 srcp에 전송
  Close(srcfd);
  Rio_writen(fd, srcp, filesize); // srcp의 내용을 fd에 작성
  // Munmap(srcp, filesize);
  free(srcp);
}



void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
  // Rio_readlineb(rp, buf, MAXLINE); // 왜 얘는 출력 안해주는 거징.. 출력해보니까 서버 주소, port 나오는 것 같긴 한데 당연해서 그런가?
  
  /* strcmp 두 문자열을 비교하는 함수 */
  /* 헤더의 마지막 줄은 비어있기에 \r\n 만 buf에 담겨있다면 while문을 탈출한다.*/
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    //멈춘 지점까지 출력하고 다시 while
  }
  return;
}



void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[]={NULL};
  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork()==0){
    /* Child process 생성 - 부모 프로세스(지금) 을 복사한*/
    /* Real server would set all CGI vars hear */
    setenv("QUERY_STRING", cgiargs, 1); //환경변수 설정
    setenv("REQUEST_METHOD", method, 1);
    Dup2(fd, STDOUT_FILENO); // 자식프로세스의 표준 출력을 연결 파일 식별자로 재지정
    Execve(filename, emptylist, environ); // filename 경로의 파일을 실행
  }
  Wait(NULL); /* Parent waits for and reaps child
  부모 프로세스가 자식 프로세스가 종료될 때까지 대기*/
}