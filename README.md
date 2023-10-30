This program is designed to take in 3 integers and 1 file name in this format:
-n [int] -s [int] -t [int] -f [filename]
-n is the number of processes to launch
-s is the number of processes to run at once
-t is the time-frequency in nanoseconds that a process will launch
-f is the name of the file to output the log
Default values: -n 5 -s 3 -t 50000000
THERE MUST BE A FILENAME ENTERED FOR THIS PROGRAM TO WORK
-n can not be more than 20
Example: ./oss -n 6 -s 5 -t 50000000 -f log.txt
This program will use its own internal clock, message queues, and scheduler to launch and manage processes. It will schedule workers "process time" dependent on its priority thats calculated by the scheduler.
When all n processes have finished, it will output a report on the OS simulation.

Git Repos: https://github.com/jhsq2r/OS_Project4
