#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define N 1000000

int array[N];
int NUM=100000;

int main()
{
	srand(time(0));
	int i;
	for(i=0;i<NUM;i++)
	{
		array[rand()%N] = i;
	}

	return 0;
}
