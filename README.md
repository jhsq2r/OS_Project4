# OS_Project4

Notes:
1. Determine if we should launch a child 1000 nano
  if (totalInsystem < s && totallaunched < numToLaunch) then launch a worker
2. Check if a blocked process should be changed to ready 2000 nano
   for x in processTable, if system clock is greater than eventwait, unblock
3. Calculate priorities of ready(running) processes 2000 nano
   for x in processTable, if occupied calculate ratio, add ratio to array
4. Schedule a process by sending it a message 1000 nano
   find lowest value in ratio list, send message to that child process
5. Receive a message back and update appropriate structures
   update system clock accordingly
   if intData == timesent, update timeinsysstem in PCB
   if intData == someNum, update timeinSystem, blocked, eventtimes, in pcb
   if intData == someNegNum, update timeinsystem, occupied, in pcb
7. Output PCB to screen and log every half second 
