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
        int serviceTimeSeconds;
        int serviceTimeNano;
        int eventWaitSec;
        int eventWaitNano;
        int blocked;
};

struct PQ {
        int pid;
        float value;
};

void displayTable(int i, struct PCB *processTable, FILE *file){
        fprintf(file,"Process Table:\nEntry Occupied PID      StartS    StartN   ServceSec   ServeNano EventSec       EventNano Blocked\n");
        printf("Process Table:\nEntry Occupied PID      StartS    StartN   ServceSec   ServeNano EventSec       EventNano Blocked\n");
        for (int x = 0; x < i; x++){
                fprintf(file,"%d      %d      %*d     %d      %*d     %d      %*d     %d      %*d     %d\n", x,processTable[x].occupied,7,processTable[x].pid,processTable[x].startSeconds,9,processTable[x].startNano,processTable[x].serviceTimeSeconds,9,processTable[x].serviceTimeNano,processTable[x].eventWaitSec,9,processTable[x].eventWaitNano,processTable[x].blocked);
                printf("%d      %d      %*d     %d      %*d     %d      %*d     %d      %*d     %d\n", x,processTable[x].occupied,7,processTable[x].pid,processTable[x].startSeconds,9,processTable[x].startNano,processTable[x].serviceTimeSeconds,9,processTable[x].serviceTimeNano,processTable[x].eventWaitSec,9,processTable[x].eventWaitNano,processTable[x].blocked);

        }
}

void help(){//Update this
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
                processTable[y].blocked = 0;
                processTable[y].serviceTimeSeconds = 0;
                processTable[y].serviceTimeNano = 0;
                processTable[y].eventWaitSec = 0;
                processTable[y].eventWaitNano = 0;
        }
        int totalInSystem = 0;
        int status;
        int totalLaunched = 0;
        int next = 0;
        int nextLaunchTime[2];
        nextLaunchTime[0] = 0;
        nextLaunchTime[1] = 0;
        int canLaunch;
        int timeTrack = 0;
        struct PQ priorityArray[10];
        int priorityArrayPosition = 0;
        long timeInSystem = 0;
        long timeInService = 0;
        long currentTimeNano = 0;
        float highestPriority = 1.5;
        int highestPriorityIndex = 0;
        int numBlocked = 0;

        while(1){
                seed++;
                srand(seed);

                if (totalLaunched != 0){
                        if (totalInSystem == 0){
                                //printf("SharedTime before %d:%d\n",sharedTime[0], sharedTime[1]);
                                sharedTime[1] += maxTime;
                                if(sharedTime[1] >= 1000000000){
                                        sharedTime[0] += 1;
                                        sharedTime[1] -= 1000000000;
                                }
                                //printf("SharedTime After %d:%d\n", sharedTime[0],sharedTime[1]);
                        }

                        if (nextLaunchTime[0] < sharedTime[0]){
                                canLaunch = 1;
                        }else if (nextLaunchTime[0] == sharedTime[0] && nextLaunchTime[1] <= sharedTime[1]){
                                canLaunch = 1;
                        }else{
                                canLaunch = 0;
                        }
                }
                //printf("Can Launch: %d\n", canLaunch);
                if((totalInSystem < simul && canLaunch == 1 && totalLaunched < proc) || totalLaunched == 0){

                        nextLaunchTime[1] = sharedTime[1] + maxTime;
                        nextLaunchTime[0] = sharedTime[0];
                        while (nextLaunchTime[1] >= 1000000000){
                                nextLaunchTime[0] = nextLaunchTime[0] + 1;
                                nextLaunchTime[1] = nextLaunchTime[1] - 1000000000;
                        }
                        //printf("Next Launch Time: %d:%d\n", nextLaunchTime[0], nextLaunchTime[1]);

                        pid_t child_pid = fork();
                        if(child_pid == 0){
                                char *args[] = {"./worker", NULL};
                                execvp("./worker", args);

                                printf("Something horrible happened...\n");
                                exit(1);
                        }else{
                                processTable[totalLaunched].occupied = 1;
                                processTable[totalLaunched].pid = child_pid;
                                processTable[totalLaunched].startSeconds = sharedTime[0];
                                processTable[totalLaunched].startNano = sharedTime[1];
                                printf("Generating process with PID %d and putting it in ready queue 0 at time %d:%d\n",child_pid,sharedTime[0],sharedTime[1]);
                                fprintf(file,"Generating process with PID %d and putting it in ready queue 0 at time %d:%d\n",child_pid,sharedTime[0],sharedTime[1]);
                        }
                        totalLaunched++;
                }
                //Add to time here
                sharedTime[1] += 10000;
                //updateTime(sharedTime, 10000);
                timeTrack += 10000;

                //Check if blocked proceccess can be unblocked
                numBlocked = 0;
                for(int y = 0; y < proc; y++){
                        if (processTable[y].blocked == 1){
                                if (sharedTime[0] > processTable[y].eventWaitSec || (sharedTime[0] == processTable[y].eventWaitSec && sharedTime[1] > processTable[y].eventWaitNano)){
                                        processTable[y].eventWaitSec = 0;
                                        processTable[y].eventWaitNano = 0;
                                        processTable[y].blocked = 0;
                                        printf("Removing prcoess with PID %d from blocked queue, adding to ready queue\n",processTable[y].pid);
                                        fprintf(file,"Removing prcoess with PID %d from blocked queue, adding to ready queue\n",processTable[y].pid);
                                }else{
                                        numBlocked++;
                                        if(numBlocked == totalInSystem){
                                                processTable[y].eventWaitSec = 0;
                                                processTable[y].eventWaitNano = 0;
                                                processTable[y].blocked = 0;
                                                printf("All processes blocked, removing process with PID %d from blocked queue, adding to ready queue\n",processTable[y].pid);
                                                fprintf(file,"All processes blocked, removing process with PID %d from blocked queue, adding to ready queue\n",processTable[y].pid);
                                        }
                                }
                        }
                }
                //Add to time here
                sharedTime[1] += 15000;
                //updateTime(sharedTime, 15000);
                timeTrack += 15000;

                //Calculate priority of ready workers, assign winners index to next, output priority array
                //printf("currentTimeNano: %d * 1B + %d\n", sharedTime[0], sharedTime[1]);
                currentTimeNano = ((long)sharedTime[0]*1000000000) + sharedTime[1];
                priorityArrayPosition = 0;
                for(int z = 0; z < proc; z++){
                        if (processTable[z].occupied == 1 && processTable[z].blocked == 0){
                                //printf("%ld - %d * 1B + %d\n",currentTimeNano,processTable[z].startSeconds,processTable[z].startNano);
                                timeInSystem = currentTimeNano - (((long)processTable[z].startSeconds * 1000000000) + processTable[z].startNano);
                                //printf("Time in System: %ld\n", timeInSystem);
                                //printf("%d * 1B + %d\n",processTable[z].serviceTimeSeconds,processTable[z].serviceTimeNano);
                                timeInService = ((long)processTable[z].serviceTimeSeconds * 1000000000) + processTable[z].serviceTimeNano;
                                //printf("Time in Service: %ld\n", timeInService);
                                if (timeInService == 0){
                                        priorityArray[priorityArrayPosition].value = 0.0;
                                }else{
                                        priorityArray[priorityArrayPosition].value = (float)timeInService/timeInSystem;
                                }
                                priorityArray[priorityArrayPosition].pid = processTable[z].pid;
                                priorityArrayPosition++;
                        }
                }
                if (priorityArrayPosition > simul){
                        printf("Simul rule broken at priority check\n");
                        return EXIT_FAILURE;
                }
                highestPriority = 1.5;
                highestPriorityIndex = 0;
                printf("Ready queue priorities: ");
                fprintf(file,"Ready queue priorities: ");
                for(int i = 0; i < priorityArrayPosition; i++){
                        printf("%.2f ",priorityArray[i].value);
                        fprintf(file,"%.2f",priorityArray[i].value);
                        if (priorityArray[i].value < highestPriority){
                                highestPriority = priorityArray[i].value;
                                highestPriorityIndex = i;
                        }
                }
                printf("\n");
                fprintf(file,"\n");
                for(int j = 0; j < proc; j++){
                        if (processTable[j].pid == priorityArray[highestPriorityIndex].pid){
                                next = j;
                                break;
                        }
                }
                //Add to time here
                sharedTime[1] += 15000;
                //updateTime(sharedTime, 15000);
                timeTrack += 15000;
                printf("Dispatching process with PID %d priority %.2f from ready queue at time %d:%d\n",priorityArray[highestPriorityIndex].pid,priorityArray[highestPriorityIndex].value,sharedTime[0],sharedTime[1]);
                fprintf(file,"Dispatching process with PID %d priority %.2f from ready queue at time %d:%d\n",priorityArray[highestPriorityIndex].pid,priorityArray[highestPriorityIndex].value,sharedTime[0],sharedTime[1]);
                printf("Total time this dispatch was %d nanoseconds\n", 15000);
                fprintf(file,"Total time this dispatch was %d nanoseconds\n", 15000);
                //send message to priority worker and log
                messenger.mtype = processTable[next].pid;
                messenger.intData = 50000000;
                if (msgsnd(msqid, &messenger, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                        perror("msgsnd to child 1 failed\n");
                        exit(1);
                }
                sharedTime[1] += 10000;
                //updateTime(sharedTime, 10000);
                timeTrack += 10000;

                //Log that message sent
                //printf("Message sent to Worker %d PID %d at time %d;%d\n", next,processTable[next].pid,sharedTime[0],sharedTime[1]);
                //fprintf(file, "Message sent to Worker %d PID %d at time %d;%d\n", next,processTable[next].pid,sharedTime[0],sharedTime[1]);

                //wait for response and log it
                if (msgrcv(msqid, &receiver, sizeof(msgbuffer), getpid(),0) == -1) {
                        perror("failed to receive message in parent\n");
                        exit(1);
                }
                //printf("Message received from Worker %d PID %d at time %d;%d\n", next,processTable[next].pid,sharedTime[0],sharedTime[1]);
                //fprintf(file,"Message received from Worker %d PID %d at time %d;%d\n", next,processTable[next].pid,sharedTime[0],sharedTime[1]);

                //Responses
                if (receiver.intData < 0){
                        //terminating
                        //printf("Worker %d PID %d says it's terminating\n", next,processTable[next].pid);
                        //fprintf(file, "Worker %d PID %d says it's terminating\n", next,processTable[next].pid);
                        waitpid(processTable[next].pid, &status, 0);
                        processTable[next].occupied = 0;
                        sharedTime[1] += (receiver.intData * -1);
                        //updateTime(sharedTime, 25000000);
                        timeTrack += (receiver.intData * -1);
                        processTable[next].serviceTimeNano += (receiver.intData * -1);
                        if(processTable[next].serviceTimeNano >= 1000000000){
                                processTable[next].serviceTimeSeconds += 1;
                                processTable[next].serviceTimeNano -= 1000000000;
                        }
                        totalInSystem = 0;
                        for (int x = 0; x < proc; x++){
                                totalInSystem += processTable[x].occupied;
                        }
                        printf("Receiving that process with PID %d ran for %d nanoseconds and is terminating\n",processTable[next].pid,(receiver.intData * -1));
                        fprintf(file,"Receiving that process with PID %d ran for %d nanoseconds and is terminating\n",processTable[next].pid,(receiver.intData * -1));
                        //printf("1Total In System: %d\n",totalInSystem);
                        if(totalInSystem == 0 && totalLaunched == proc){
                                break;
                        }

                }else if(receiver.intData == 50000000){
                        //not done but used full time
                        //printf("Worker %d PID %d says it's not done yet(Used all time)\n", next,processTable[next].pid);
                        //fprintf(file, "Worker %d PID %d says it's not done yet\n", next,processTable[next].pid);
                        sharedTime[1] += 50000000;
                        //updateTime(sharedTime, 50000000);
                        timeTrack += 50000000;
                        processTable[next].serviceTimeNano += 50000000;
                        if(processTable[next].serviceTimeNano >= 1000000000){
                                processTable[next].serviceTimeSeconds += 1;
                                processTable[next].serviceTimeNano -= 1000000000;
                        }
                        printf("Receiving that process with PID %d ran for %d nanoseconds\n",processTable[next].pid,50000000);
                        fprintf(file,"Receiving that process with PID %d ran for %d nanoseconds\n",processTable[next].pid,50000000);
                        printf("Putting process with PID %d into ready queue\n",processTable[next].pid);
                        fprintf(file,"Putting process with PID %d into ready queue\n",processTable[next].pid);
                }else if(receiver.intData < 50000000 && receiver.intData > 0){
                        //blocked by event
                        //printf("Process Blocked by event\n");
                        sharedTime[1] += receiver.intData;
                        //updateTime(sharedTime, 25000000);
                        timeTrack += receiver.intData;
                        printf("Receiving that process with PID %d ran for %d nanoseconds: did not use entire time quantum\n",processTable[next].pid,receiver.intData);
                        fprintf(file,"Receiving that process with PID %d ran for %d nanoseconds: did not use entire time quantum\n",processTable[next].pid,receiver.intData);
                        printf("Putting process with PID %d into blocked queue\n",processTable[next].pid);
                        fprintf(file,"Putting process with PID %d into blocked queue\n",processTable[next].pid);
                        processTable[next].serviceTimeNano += receiver.intData;
                        if(processTable[next].serviceTimeNano >= 1000000000){
                                processTable[next].serviceTimeSeconds += 1;
                                processTable[next].serviceTimeNano -= 1000000000;
                        }
                        processTable[next].eventWaitSec = (rand() % 2) + sharedTime[0];
                        processTable[next].eventWaitNano = ((rand() % (999999999 - 1 + 1)) + 1) + sharedTime[1];
                        if(processTable[next].eventWaitNano >= 1000000000){
                                processTable[next].eventWaitSec += 1;
                                processTable[next].eventWaitNano -= 1000000000;
                        }
                        processTable[next].blocked = 1;
                }else{
                        printf("Something weird happened\n");
                        break;
                }
                //printf("DATA RECEIVED FROM LAST MESSAGE: %d\n", receiver.intData);

                if(sharedTime[1] >= 1000000000){
                        sharedTime[0] += 1;
                        sharedTime[1] -= 1000000000;
                }
                //display table here
                if (timeTrack >= 500000000){
                        displayTable(totalLaunched, processTable, file);
                        timeTrack = 0;
                }
                //Calculate total in system
                totalInSystem = 0;
                for (int x = 0; x < proc; x++){
                        totalInSystem += processTable[x].occupied;
                }
                //printf("2Total In System: %d\n", totalInSystem);

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
