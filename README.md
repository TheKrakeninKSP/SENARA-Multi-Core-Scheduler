# SENARA-Multi-Core-Scheduler
An asymmetric multi-core process scheduler with testbench and customizable parameters

//Multi-Processor Scheduler  --- SENARA Scheduling Algorithm

//3 types of cores -- Long Process [LP], General Purpose [GP], Fast Response [FR]

    //Long Proc cores run processes with large burst times, ideally background algorithms
        //LP Cores also have higher compute power and use more power
    //Fast Response cores preferably run GUI front-ends and such
        //FR Cores have lower compute power and low power usage
        
//Symmetric Multiprocessing Approach has issues with Imbalanced Load -- https://en.wikipedia.org/wiki/Symmetric_multiprocessing
//Assume Shared Main Memory via a Common System Bus but Separate Cache for each Core
    //Resolving Cache Coherence is the System Bus' Responsibility
//Asymmetric because the cores are not all equal --> Core specialization -- https://en.wikipedia.org/wiki/ARM_big.LITTLE

//Process table is generated for the given parameters and the saved as a file
//Process table is then read from memory and loaded in to the initial global queue
//global queue : all new processes initially arrive at the Global queue
//LP Buffer: processes waiting to be assigned to a LP core
//FR Buffer: processes waiting to be assigned to a FR core
//global scheduler assigns 
    //1.the processes in LP Buffer (if any) are assigned to LP cores
    //2.the processes in FR Buffer (if any) are assigned to FR cores
    //3.the processes in global queue are assigned to one of the GP cores's local queues
        //assign to GP core with smallest number of proc in local queue
    //scheduler repeats these 3 steps until all processes terminate

//track the cumulative burst time for each process
//if burst time crosses LONG_PROC_THRESH, then process is a long process --> add to LP Buffer
    //expect the global scheduler to assign the process to a LP core

//track ratio of [number of voluntary_I/O_request_preemptions] to [cumulative burst_time] for each process  --> interaction_ratio
    //if interaction_ratio is high, remove from GP core and move to GP buffer
    //expect the global scheduler to assign the process to a GP core

//FR Cores are low power, low performace
//LP Cores are high power, high performance
//GP Cores are balanced power and performance

//user can choose Between Power-Saving, Balanced and Performance Mode -- this affects scheduling
    //Power-Saving uses GP Cores more, minimizes LP Cores and FR Cores
    //Balanced is an even distrubution of cores, but doesn't activate all cores
    //Performace -- All Cores active, MAX power use


//NOTE: win32 Time Slice is approx 20ms

