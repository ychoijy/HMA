#include "ptlcalls.h"
#include <stdio.h>

int main()
{
  printf("Simulation end\n");
  ptlcall_switch_to_native();
  return 0;
}
