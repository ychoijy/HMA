#include <sys/types.h>

#include <unistd.h>

#include <stdio.h>

#include <stdlib.h>

#define CHILD_NUM		1
#define MEMORY_ALLOC_NUM	100
#define MEMORY_ALLOC_SIZE	30*1024*1024
#define LOOP_NUM		30

int main(void)
{
	int i;
	pid_t pid, ppid;
	int intTmp, intTmp2;
	int intChildsCt = CHILD_NUM;
	int intChilds[CHILD_NUM];
	int intMemAllocCt = MEMORY_ALLOC_NUM;
	char *cpStr[MEMORY_ALLOC_NUM];
	int intMemSize=MEMORY_ALLOC_SIZE;
	int intPageCt = intMemSize/4096;
	ppid = getpid();
	pid = 1;

	for (intTmp=0; intTmp < intChildsCt; intTmp++) {
		if (pid != 0) { // 부모만 fork()가 실행되도록
			if ((pid=fork()) < 0) {
				printf("%d'th child process fork error\n", intTmp);
				return 2;
			} else if (pid != 0) { // 부모는 pid를 배열에 저장.
				intChilds[intTmp] = pid;
			}
		}
	}
#if 0
	if ( pid != 0 ){
		printf ("==== ChildProcess List====\n");
		for (intTmp=0; intTmp < intChildsCt; intTmp++) {
			printf ("%d'st Child process pid : %d\n", intTmp, intChilds[intTmp]);
		}
	}
#endif
	if ( pid == 0 ) {
		printf ("Child Routine...\n");
		for (intTmp2=0; intTmp2<intMemAllocCt; intTmp2++) {
			cpStr[intTmp2] = (char *)malloc(intMemSize);
			printf("Child %d , Memory Allocate : %d\n", getpid(), intTmp2);
		}
		printf("ppid = %d, getpid = %d\n", ppid, getpid());

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
	}
	return 0;
}
