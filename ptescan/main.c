#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define N 1000000

int array[N];

int NUM=100000;

int main()
{
	int i, j;

	srand(time(0));

	for(i=0;i<NUM;i++)
	{
		j = rand() % N
		array[j] = i;
	}

	return 0;
}
