#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>

#define SHMKEY 55555
#define PERMS 0644
#define MSGKEY 66666

typedef struct msgbuffer {
        long mtype;
        int intData;
} msgbuffer;

int main(int argc, char** argv){

        msgbuffer receiver;
        receiver.mtype = 1;
        int msqid = 0;

        if ((msqid = msgget(MSGKEY, PERMS)) == -1) {
                perror("msgget in child");
                exit(1);
        }

        int shmid = shmget(SHMKEY, sizeof(int)*2, 0777);
        if(shmid == -1){
                printf("Error in shmget\n");
                return EXIT_FAILURE;
        }
        int * sharedTime = (int*) (shmat (shmid, 0, 0));

        //printf("This is Child: %d, From Parent: %d, Seconds: %s, NanoSeconds: %s\n", getpid(), getppid(), argv[1], argv[2]);

        //printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d JUST STARTING\n",getpid(),getppid(),sharedTime[0],sharedTime[1],exitTime[0],exitTime[1]);

        srand(time(NULL));
        int seed = rand();
        int percentChance;
        int givenTime;
        while (1){
                seed++;
                //wait for message
                if ( msgrcv(msqid, &receiver, sizeof(msgbuffer), getpid(), 0) == -1) {
                        perror("failed to receive message from parent\n");
                        exit(1);
                }
                //printf("WORKER PID:%d Message received\n",getpid());

                givenTime = receiver.intData;
                srand(seed*getpid());
                percentChance = (rand() % 100) + 1;
                //printf("PercentChance: %d\n", percentChance);
                if (percentChance <= 5){//Blocked send percent of time used
                        receiver.intData = (rand() % (givenTime - 1)) + 1;
                }else if (percentChance > 5 && percentChance < 97){//Send all time back
                        receiver.intData = givenTime;
                }else if (percentChance >= 97 && percentChance <= 100){//Terminate send percent of time back
                        receiver.intData = ((rand() % (givenTime - 1)) + 1) * -1;
                        break;
                }else{
                        printf("Something went wrong: Random number %d\n", percentChance);
                        receiver.intData = 4;
                        break;
                }

                receiver.mtype = getppid();
                if (msgsnd(msqid, &receiver, sizeof(msgbuffer)-sizeof(long),0) == -1){
                        perror("msgsnd to parent failed\n");
                        exit(1);
                }

        }

        receiver.mtype = getppid();
        if (msgsnd(msqid, &receiver, sizeof(msgbuffer)-sizeof(long),0) == -1){
                perror("msgsnd to parent failed\n");
                exit(1);
        }

        //printf("Killing child %d\n", getpid());
        //printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d TERMINATING\n",getpid(),getppid(),sharedTime[0],sharedTime[1],exitTime[0],exitTime[1]);
        shmdt(sharedTime);

        return 0;

}
