#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define N 10000000

int array[N];

int main()
{
	int i, j;

	srand(time(0));
	while(1){
		for(i=0;i<N;i++)
		{
			j = rand() % N;
			array[j] = i;
		}
	}
	return 0;
}
