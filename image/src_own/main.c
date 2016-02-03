#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ONE_MEGA 1024*1024
#define MAX_SIZE 10*ONE_MEGA


int *list;
int *sorted;

void merge(int *list, int left, int mid, int right)

{

	int i, j, k, l;

	i=left;  j=mid+1;  k=left;

	while(i<=mid && j<=right){

		if(list[i]<=list[j])

			sorted[k++] = list[i++];

		else

			sorted[k++] = list[j++];

	}

	if(i>mid)

		for(l=j; l<=right; l++)

			sorted[k++] = list[l];

	else

		for(l=i; l<=mid; l++)

			sorted[k++] = list[l];



	for(l=left; l<=right; l++)

		list[l] = sorted[l];

}



void merge_sort(int list[], int left, int right)

{

	int mid;

	if(left<right){

		mid = (left+right)/2;

		merge_sort(list, left, mid);

		merge_sort(list, mid+1, right);

		merge(list, left, mid, right);

	}

}

void main()

{

	int i, n = MAX_SIZE;

	srand(time(NULL));

	list = (int *)malloc(sizeof(int)*n);
	sorted = (int *)malloc(sizeof(int)*n);

	for (i=0; i<n; i++) {
		list[i] = rand()%n;
	}

	merge_sort(list, 0, n-1);
/*
	for(i=0; i<n; i++)
		printf("%d\n", list[i]);
*/
	free(list);
	free(sorted);
}
