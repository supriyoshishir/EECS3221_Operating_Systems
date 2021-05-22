#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <semaphore.h>

void logStart(char* tID);//function to log that a new thread is started
void logFinish(char* tID);//function to log that a thread has finished its time
void startClock();//function to start program clock
long getCurrentTime();//function to check current time since clock was started
time_t programClock;//the global timer/clock for the program

typedef struct semaphore {
	sem_t semEven;
	sem_t semOdd;
	int evenCounter;
	int oddCounter;
} Semaphore;
Semaphore sema = {0};

typedef struct thread
{
	char tid[4];
	unsigned int startTime;
	int state;
	pthread_t handle;
	int retVal;
	int odd;
} Thread;

int threadsLeft(Thread* threads, int threadCount);
int threadToStart(Thread* threads, int threadCount);
void* threadRun(void* t);//the thread function, the code executed by each thread
int readFile(char* fileName, Thread** threads);//function to read the file content and build array of threads
void isDeadlock(); // function for deadlocks

int main(int argc, char *argv[])
{
	if(argc<2)
	{
		printf("Input file name missing...exiting with error code -1\n");
		return -1;
	}

	Thread* myThreads = NULL;
	int threadCount = readFile(argv[1],&myThreads);

	int et = myThreads[0].startTime;
	int etIndex = 0;
	int lt = myThreads[0].startTime;

	for (int i = 0; i < threadCount; i++){
		if (myThreads[i].startTime >= lt){
			lt = myThreads[i].startTime;
		}
		if (myThreads[i].startTime < et){
			et = myThreads[i].startTime;
			etIndex = i;
		}
	}

	if (myThreads[etIndex].odd){
		sem_init(&sema.semEven, 0, 1);
		sem_init(&sema.semOdd, 0, 0);
	} else {
		sem_init(&sema.semEven, 0, 0);
		sem_init(&sema.semOdd, 0, 1);
	}

	startClock();
	while(threadsLeft(myThreads, threadCount)>0)
	{
		int idx = threadToStart(myThreads, threadCount);
		while(idx>-1)
		{
		   
			if (myThreads[idx].odd == 1) {
				sema.evenCounter--;
			} else {
				sema.oddCounter--;
			}

			myThreads[idx].state = 1;
			myThreads[idx].retVal = pthread_create(&(myThreads[idx].handle),NULL,threadRun,&myThreads[idx]);
			idx = threadToStart(myThreads, threadCount);
		}

		if (getCurrentTime() == lt){
			isDeadlock();
		}

	}
	return 0;
}


void isDeadlock(){
	if (sema.evenCounter == 0){
		sem_post(&sema.semOdd);
	}

	if (sema.oddCounter == 0){
		sem_post(&sema.semEven);
	}
}

int readFile(char* fileName, Thread** threads)
{
	FILE *in = fopen(fileName, "r");
	if(!in)
	{
		printf("Child A: Error in opening input file...exiting with error code -1\n");
		return -1;
	}

	struct stat st;
	fstat(fileno(in), &st);
	char* fileContent = (char*)malloc(((int)st.st_size+1)* sizeof(char));
	fileContent[0] = '\0';
	while(!feof(in))
	{
		char line[100];
		if(fgets(line,100,in)!=NULL)
		{
			strncat(fileContent,line,strlen(line));
		}
	}
	fclose(in);

	char* command = NULL;
	int threadCount = 0;
	char* fileCopy = (char*)malloc((strlen(fileContent)+1)*sizeof(char));
	strcpy(fileCopy,fileContent);
	command = strtok(fileCopy,"\r\n");
	while(command!=NULL)
	{
		threadCount++;
		command = strtok(NULL,"\r\n");
	}
	
	*threads = (Thread*) malloc(sizeof(Thread)*threadCount);

	char* lines[threadCount];
	command = NULL;
	int i=0;
	command = strtok(fileContent,"\r\n");
	while(command!=NULL)
	{
		lines[i] = malloc(sizeof(command)*sizeof(char));
		strcpy(lines[i],command);
		i++;
		command = strtok(NULL,"\r\n");
	}

	for(int k=0; k<threadCount; k++)
	{
		char* token = NULL;
		int j = 0;
		token =  strtok(lines[k],";");
		while(token!=NULL)
		{
//if you have extended the Thread struct then here
//you can do initialization of those additional members
//or any other action on the Thread members
			(*threads)[k].state=0;
			if(j==0)
				strcpy((*threads)[k].tid,token);
			if(j==1)
				(*threads)[k].startTime=atoi(token);
			j++;
			token = strtok(NULL,";");
		}

		// check each thread if it is odd or not
		 if ((*threads)[k].tid[2]-'0' % 2 == 0){
			 (*threads)[k].odd = 1;
			 sema.evenCounter++;
		 } else {
			 (*threads)[k].odd = 0;
			 sema.oddCounter++;
		 }

	}
	return threadCount;
}

void logStart(char* tID)
{
	printf("[%ld] New Thread with ID %s is started.\n", getCurrentTime(), tID);
}

void logFinish(char* tID)
{
	printf("[%ld] Thread with ID %s is finished.\n", getCurrentTime(), tID);
}

int threadsLeft(Thread* threads, int threadCount)
{
	int remainingThreads = 0;
	for(int k=0; k<threadCount; k++)
	{
		if(threads[k].state>-1)
			remainingThreads++;
	}
	return remainingThreads;
}

int threadToStart(Thread* threads, int threadCount)
{
	for(int k=0; k<threadCount; k++)
	{
		if(threads[k].state==0 && threads[k].startTime==getCurrentTime())
			return k;
	}
	return -1;
}

void* threadRun(void* t)
{
	logStart(((Thread*)t)->tid);

	if (((Thread*)t)->odd){
		sem_wait(&sema.semEven);
	} else {
		sem_wait(&sema.semOdd);
	}

	printf("[%ld] Thread %s is in its critical section\n",getCurrentTime(), ((Thread*)t)->tid);
	
	if (((Thread*)t)->odd){
		sem_post(&sema.semOdd);
	} else {
		sem_post(&sema.semEven);
	}

	logFinish(((Thread*)t)->tid);
	((Thread*)t)->state = -1;
	pthread_exit(0);
}

void startClock()
{
	programClock = time(NULL);
}

long getCurrentTime()
{
	time_t now;
	now = time(NULL);
	return now-programClock;
}
