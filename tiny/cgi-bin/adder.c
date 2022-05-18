/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /* CGI 환경변수인 QUERY_STRING에서 인자 받앙오기 */
  if ((buf = getenv("QUERY_STRING")) != NULL) { // 환경변수에 들어 있는 값 읽어오기
    p = strchr(buf, '&');   // buf 스트링에서 &문자가 있는 위치 포인터 리턴(쿼리 스트링 인자 쪼개기 위해서)
    *p = '\0';              // 쿼리 스트링의 '&' 문자를 종료문자로 바꿔주기
    strcpy(arg1, buf);      // buf의 string을 읽어서 arg1에 copy
    strcpy(arg2, p+1);      // &의 다음 문자부터 읽어서 arg2에 copy
    strcpy(arg1, strchr(arg1, '=') + 1); // 'First=123'이런 꼴이어서 숫자만 copy
    strcpy(arg2, strchr(arg2, '=') + 1);
    n1 = atoi(arg1); // ASCII to Integer
    n2 = atoi(arg2);
  }

  /* response 출력문 만들기 */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!!!!\r\n", content);


  /* HTTP 응답 생성 */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n"); // head와 body를 구분하기 위해 엔터 두 번
  // 여기까지의 print가 HTTP response의 header이고 아래부터 html인듯?
  // printf("hello!!!! \r\n");

  printf("%s", content);
  // fflush(stdout);

  exit(0);
}
/* $end adder */
