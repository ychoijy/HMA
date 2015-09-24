#include <stdio.h>
#include <stdlib.h>

#define MAX_SIZE 99999999

int list[MAX_SIZE];

int sorted[MAX_SIZE];

void merge(int list[], int left, int mid, int right)

{

	int i, j, k, l;

	i=left;  j=mid+1;  k=left;

	while(i<=mid && j<=right){

		if(list[i]<=list[j])

			sorted[k++] = list[i++];

		else

			sorted[k++] = list[j++];

	}

	if(i>mid)	/* ³²ŸÆ ÀÖŽÂ ·¹ÄÚµåÀÇ ÀÏ°ý º¹»ç */

		for(l=j; l<=right; l++)

			sorted[k++] = list[l];

	else	/* ³²ŸÆ ÀÖŽÂ ·¹ÄÚµåÀÇ ÀÏ°ý º¹»ç */

		for(l=i; l<=mid; l++)

			sorted[k++] = list[l];

	/* ¹è¿­ sorted[]ÀÇ ž®œºÆ®žŠ ¹è¿­ list[]·Î Àçº¹»ç */

	for(l=left; l<=right; l++)

		list[l] = sorted[l];

}

//

void merge_sort(int list[], int left, int right)

{

	int mid;

	if(left<right){

		mid = (left+right)/2;     /* ž®œºÆ®ÀÇ ±Õµî ºÐÇÒ */

		merge_sort(list, left, mid);    /* ºÎºÐ ž®œºÆ® Á€·Ä */

		merge_sort(list, mid+1, right); /* ºÎºÐ ž®œºÆ® Á€·Ä */

		merge(list, left, mid, right);    /* ÇÕºŽ */

	}

}

void main()

{

	int i, n = MAX_SIZE;
	int *list;

	list = (int *)malloc(sizeof(int)*n);

	for(i=0; i<n; i++)      	/* ³­Œö »ýŒº ¹× Ãâ·Â */

		list[i] = rand()%n;/*³­Œö ¹ß»ý ¹üÀ§ 0~MAX_SIZE*/

	merge_sort(list, 0, n-1);

	for(i=0; i<n; i++)

		printf("%d\n", list[i]);

	free(list);
}
