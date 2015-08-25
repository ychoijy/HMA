#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#define MEMORY_ALLOC_NUM	100
#define MEMORY_ALLOC_SIZE	10*1024*1024
#define LOOP_NUM		50
#define SLEEP_TIME		180

struct sigaction act_new;
struct sigaction act_old;

char *cpStr[MEMORY_ALLOC_NUM];

void sigint_handler(int signo)
{
	int intTmp2;

	for (intTmp2=0; intTmp2<MEMORY_ALLOC_NUM; intTmp2++) {
		free(cpStr[intTmp2]);
		printf("pid=%d, %d is free...\n", getpid(), intTmp2);
	}
	sigaction(SIGINT, &act_old, NULL);
}

int main(void)
{
	int i;
	int intTmp, intTmp2;
	int intMemSize=MEMORY_ALLOC_SIZE;
	int intPageCt = intMemSize/4096;

	act_new.sa_handler = sigint_handler;
	sigemptyset(&act_new.sa_mask);


	for (intTmp2 = 0; intTmp2 < MEMORY_ALLOC_NUM; intTmp2++) {
		cpStr[intTmp2] = (char *)malloc(intMemSize);
	}

	for (i=0;i<LOOP_NUM;i++){
		for (intTmp2=0; intTmp2 < MEMORY_ALLOC_NUM; intTmp2++) {
			for (intTmp=1; intTmp<intPageCt; intTmp++) {
				*((char *)cpStr[intTmp2]+(4096*intTmp)) = 'K';
			}
		}
		sleep (1);
	}

	printf("Now~ %d seconds sleep!!!!\n", SLEEP_TIME);

	sigaction(SIGINT, &act_new, &act_old);
	for ( i=0;i<SLEEP_TIME;i++) {
		printf("%d / %d seconds\n", i, SLEEP_TIME);
		sleep(1);
	}

	return 0;
}
