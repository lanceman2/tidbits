#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hello.h"

void hello(void)
{
  if(strcmp(NAME,"@name@") == 0)
    exit(1);

  printf("hello\n");
}
