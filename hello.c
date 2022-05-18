#include <stdio.h>
#include <string.h>

int main() {
  char p[60] = "hello my friend";
  char q[100];
  char *temp = NULL;

  printf("%s\n", p);
  temp = strchr(p, 'm');
  printf("temp : %s\n", temp);

  strcpy(q, strchr(p, 'm'));
  strcpy(q, strchr(q, 'f')+1);
  printf("strcpy 결과 : %s \n", q);


  return 0;
}