#include <sys/types.h>

#include <unistd.h>

#include <stdio.h>

#include <stdlib.h>

#define MEMORY_ALLOC_NUM	100
#define MEMORY_ALLOC_SIZE	30*1024*1024
#define LOOP_NUM		40

int main(void)
{
	int i;
	int intTmp, intTmp2;
	int intMemAllocCt = MEMORY_ALLOC_NUM;
	char *cpStr[MEMORY_ALLOC_NUM];
	int intMemSize=MEMORY_ALLOC_SIZE;
	int intPageCt = intMemSize/4096;

	for (intTmp2=0; intTmp2<intMemAllocCt; intTmp2++) {
		cpStr[intTmp2] = (char *)malloc(intMemSize);
	}

	for (i=0;i<LOOP_NUM;i++){
		for (intTmp2=0; intTmp2<intMemAllocCt; intTmp2++) {
			for (intTmp=1; intTmp<intPageCt; intTmp++) {
				*((char *)cpStr[intTmp2]+(4096*intTmp)) = 'K';
			}
		}
		sleep (1);
	}

	for (intTmp2=0; intTmp2<intMemAllocCt; intTmp2++) {
		free(cpStr[intTmp2]);
		printf("pid=%d, %d is free...\n", getpid(), intTmp2);
	}
	return 0;
}
