#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

//Creator: Jarod Stagner
//Turn-in date:

#define SHMKEY 55555
#define PERMS 0644
#define MSGKEY 66666

typedef struct msgbuffer {
        long mtype;
        int intData;
} msgbuffer;

static void myhandler(int s){
        printf("Killing all... exiting...\n");
        kill(0,SIGTERM);

}

static int setupinterrupt(void) {
                struct sigaction act;
                act.sa_handler = myhandler;
                act.sa_flags = 0;
                return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}
static int setupitimer(void) {
                struct itimerval value;
                value.it_interval.tv_sec = 60;
                value.it_interval.tv_usec = 0;
                value.it_value = value.it_interval;
                return (setitimer(ITIMER_PROF, &value, NULL));
}

struct PCB {
        int occupied;
        pid_t pid;
        int startSeconds;
        int startNano;
};

void displayTable(int i, struct PCB *processTable, FILE *file){
        fprintf(file,"Process Table:\nEntry Occupied PID StartS StartN\n");
        printf("Process Table:\nEntry Occupied PID StartS StartN\n");
        for (int x = 0; x < i; x++){
                fprintf(file,"%d        %d      %d      %d      %d\n",x,processTable[x].occupied,processTable[x].pid,processTable[x].startSeconds,processTable[x].startNano);
                printf("%d      %d      %d      %d      %d\n", x,processTable[x].occupied,processTable[x].pid,processTable[x].startSeconds,processTable[x].startNano);

        }
}

void updateTime(int *sharedTime){
        sharedTime[1] = sharedTime[1] + 100000000;
        if (sharedTime[1] >= 1000000000 ){
                sharedTime[0] = sharedTime[0] + 1;
                sharedTime[1] = sharedTime[1] - 1000000000;
        }
}

void help(){
        printf("Program usage\n-h = help\n-n [int] = Num Children to Launch\n-s [int] = Num of children allowed at once\n-t [int] = Max num of seconds for each child to be alive\n-f [filename] = name of file to write log to");
        printf("Default values are -n 5 -s 3 -t 3\nThis Program is designed to take in 4 inputs for Num Processes, Num of processes allowed at once,\nMax num of seconds for each process, and the name of a file to write the log to");
        printf("\nThis program requires a filename to be entered");
}

int main(int argc, char** argv) {

        msgbuffer messenger;
        msgbuffer receiver;
        int msqid;

        if ((msqid = msgget(MSGKEY, PERMS | IPC_CREAT)) == -1) {
                perror("msgget in parent");
                exit(1);
        }

        if (setupinterrupt() == -1) {
                perror("Failed to set up handler for SIGPROF");
                return 1;
        }
        if (setupitimer() == -1) {
                perror("Failed to set up the ITIMER_PROF interval timer");
                return 1;
        }

        srand(time(NULL));
        int seed = rand();
        int proc = 5;
        int simul = 3;
        int maxTime = 3;//default parameters
        FILE *file;

        int shmid = shmget(SHMKEY, sizeof(int)*2, 0777 | IPC_CREAT);
        if(shmid == -1){
                printf("Error in shmget\n");
                return EXIT_FAILURE;
        }
        int * sharedTime = (int *) (shmat (shmid, 0, 0));
        sharedTime[0] = 0;
        sharedTime[1] = 0;

        int option;
        while((option = getopt(argc, argv, "hn:s:t:f:")) != -1) {//Read command line arguments
                switch(option){
                        case 'h':
                                help();
                                return EXIT_FAILURE;
                                break;
                        case 'n':
                                proc = atoi(optarg);
                                break;
                        case 's':
                                simul = atoi(optarg);
                                break;
                        case 't':
                                maxTime = atoi(optarg);
                                break;
                        case 'f':
                                file = fopen(optarg,"w");
                                break;
                        case '?':
                                help();
                                return EXIT_FAILURE;
                                break;
                }
        }

        fprintf(file,"Ran with arguments -n %d -s %d -t %d \n", proc,simul,maxTime);


        struct PCB processTable[20];
        for (int y = 0; y < 20; y++){
                processTable[y].occupied = 0;
        }
        int total = 0;
        int status;
        int i = 0;
        int next = 0;
        while(1){
                seed++;
                srand(seed);
                updateTime(sharedTime);
                if (sharedTime[1] == 500000000 || sharedTime[1] == 0){
                        displayTable(i, processTable, file);
                }
                total=0;
                for(int z = 0; z < proc; z++){
                        total += processTable[z].occupied;
                }
                if(total < simul && i < proc){
                        pid_t child_pid = fork();
                        if(child_pid == 0){
                                char convertSec[20];
                                char convertNan[20];

                                int randomSec = (rand() % ((maxTime - 1) -1 + 1)) + 1;
                                int randomNano = (rand() % (999999999 - 1 + 1)) + 1;

                                sprintf(convertSec, "%d", randomSec);
                                sprintf(convertNan, "%d", randomNano);

                                char *args[] = {"./worker", convertSec, convertNan, NULL};
                                execvp("./worker", args);

                                printf("Something horrible happened...\n");
                                exit(1);
                        }else{
                                processTable[i].occupied = 1;
                                processTable[i].pid = child_pid;
                                processTable[i].startSeconds = sharedTime[0];
                                processTable[i].startNano = sharedTime[1];
                        }
                        i++;
                }
                while (processTable[next].occupied != 1){
                        next++;
                        if (next == 20){
                                next = 0;
                        }
                }

                //send message to next and log
                messenger.mtype = processTable[next].pid;
                messenger.intData = processTable[next].pid;

                if (msgsnd(msqid, &messenger, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                        perror("msgsnd to child 1 failed\n");
                        exit(1);
                }
                //Log that message sent
                printf("Message sent to Worker %d PID %d at time %d;%d\n", next,processTable[next].pid,sharedTime[0],sharedTime[1]);
                fprintf(file, "Message sent to Worker %d PID %d at time %d;%d\n", next,processTable[next].pid,sharedTime[0],sharedTime[1]);
                //wait for response and log it
                if (msgrcv(msqid, &receiver, sizeof(msgbuffer), getpid(),0) == -1) {
                        perror("failed to receive message in parent\n");
                        exit(1);
                }
                printf("Message received from Worker %d PID %d at time %d;%d\n", next,processTable[next].pid,sharedTime[0],sharedTime[1]);
                fprintf(file,"Message received from Worker %d PID %d at time %d;%d\n", next,processTable[next].pid,sharedTime[0],sharedTime[1]);
                if (receiver.intData == 0){
                        //terminating
                        printf("Worker %d PID %d says it's terminating\n", next,processTable[next].pid);
                        fprintf(file, "Worker %d PID %d says it's terminating\n", next,processTable[next].pid);
                        waitpid(processTable[next].pid, &status, 0);
                        processTable[next].occupied = 0;
                        total=0;
                        for(int u = 0; u < proc; u++){
                                total += processTable[u].occupied;
                        }
                        if (total == 0){
                                break;
                        }
                }else if(receiver.intData == 1){
                        //not done yet
                        printf("Worker %d PID %d says it's not done yet\n", next,processTable[next].pid);
                        fprintf(file, "Worker %d PID %d says it's not done yet\n", next,processTable[next].pid);
                }else{
                        //something bad happened
                        printf("Something weird happened\n");
                }
                next++;
                if (next == 20){
                        next = 0;
                }

        }

        displayTable(proc, processTable, file);

        shmdt(sharedTime);
        shmctl(shmid,IPC_RMID,NULL);
        if (msgctl(msqid, IPC_RMID, NULL) == -1) {
                perror("msgctl to get rid of queue in parent failed");
                exit(1);
        }

        printf("Done\n");
        fprintf(file, "Done\n");
        fclose(file);

        return 0;
}
