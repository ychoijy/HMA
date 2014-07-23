#include "ptlcalls.h"
#include <stdio.h>

int main()
{
  printf("Simulation killed\n");
  ptlcall_kill();
  return 0;
}
