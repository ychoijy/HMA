#include "ptlcalls.h"
#include <stdio.h>

int main()
{
  printf("Switching to simulation\n");
  ptlcall_switch_to_sim();
  return 0;
}
