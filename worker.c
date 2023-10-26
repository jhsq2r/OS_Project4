#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

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

        int exitTime[2];
        exitTime[0] = atoi(argv[1]) + sharedTime[0];
        exitTime[1] = atoi(argv[2]) + sharedTime[1];
        if (exitTime[1] >= 1000000000){
                exitTime[1] -= 1000000000;
                exitTime[0] += 1;
        }

        printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d JUST STARTING\n",getpid(),getppid(),sharedTime[0],sharedTime[1],exitTime[0],exitTime[1]);

        int currentTime = sharedTime[0];
        int secondsPassed = 0;
        while (1){
                //wait for message
                if ( msgrcv(msqid, &receiver, sizeof(msgbuffer), getpid(), 0) == -1) {
                        perror("failed to receive message from parent\n");
                        exit(1);
                }
                printf("WORKER PID:%d Message received\n",getpid());
                if (exitTime[0] > sharedTime[0] || exitTime[1] > sharedTime[1]){
                        if(sharedTime[0] - exitTime[0] >= 2){
                                break;
                        }
                        if(currentTime != sharedTime[0]){
                                secondsPassed += sharedTime[0]-currentTime;
                                printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d, %d SECONDS HAVE PASSED\n",getpid(),getppid(),sharedTime[0],sharedTime[1],exitTime[0],exitTime[1],secondsPassed);
                                currentTime = sharedTime[0];
                        }
                        //send message saying not terminating
                        receiver.mtype = getppid();
                        receiver.intData = 1;
                        if (msgsnd(msqid, &receiver, sizeof(msgbuffer)-sizeof(long),0) == -1){
                                perror("msgsnd to parent failed\n");
                                exit(1);
                        }

                }else{
                        break;
                }
        }
        printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d TERMINATING\n",getpid(),getppid(),sharedTime[0],sharedTime[1],exitTime[0],exitTime[1]);
        receiver.mtype = getppid();
        receiver.intData = 0;
        if (msgsnd(msqid, &receiver, sizeof(msgbuffer)-sizeof(long),0) == -1){
                perror("msgsnd to parent failed\n");
                exit(1);
        }

        shmdt(sharedTime);

        return 0;

}
