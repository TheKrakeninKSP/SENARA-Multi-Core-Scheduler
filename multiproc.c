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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define DEBUG 0             //1 for Printing Detailed Info, 0 Otherwise
#define MODE PERFORMANCE    //chose between POWER_SAVING, PERFORMANCE, and BALANCED
int NAIVE = 0;              //1 for using Naive scheduler, 0 for Advanced Scheduler, overridden by Command Line Argument
int NEW_SCHEDULE = 0;       //1 for Creating New Schedule, 0 To reuse previously created Schedule

//DO NOT CHANGE
#define LP 444 // Long Process core ID
#define FR 333 // Fast Response core ID
#define GP 555 // General Purpose core ID
#define FB 666 //FR Buffer
#define LB 777 //LP Buffer
#define GL 111 //Global Queue

#define FASTRESPONSE "FAST RESP"
#define LONGPROCESS "PERFORMANCE"
#define GENERAL "STANDARD"

#define LONGPROC "LONG"
#define FASTPROC "I/O HEAVY"
#define GENLPROC "GENERAL"

#define SCHEDULE "schedule.txt"
#define BUFFSIZE 200


//CUSTOMIZE WITH CAUTION
#define LONG_PROC_THRESH 200000 // 200ms
#define INTER_RATIO_THRESH 1.1 // ratio threshold for interaction
#define CHAIN_MAX_ITEMS 11 //Odd number so that Process doesn't end with IO

#define LP_COMPUTE 60 //compute units -- also represents the wattage of the CPU
#define GP_COMPUTE 50  
#define FR_COMPUTE 30
#define MAX_COMPUTE 200 //max compute requirement for a single item in the process chain for General Process

#define LP_COMPUTE_LOWERBOUND 250
#define LP_COMPUTE_UPPERBOUND 500
#define FR_COMPUTE_UPPERBOUND 30

#define STD_TIMESLICE 20000 //microseconds
#define ARRIVAL_TIME_MAX_INCREMENT 10000 //microseconds

//reducing timeslice with compute staying constant represents an increasing in processing speed
#define LP_TIMESLICE_MULT 3
#define GP_TIMESLICE_MULT 2
#define FR_TIMESLICE_MULT 1


//CUSTOMIZE AS REQUIRED 
#define MAX_PROCESSES 100
#define LPMAX 5 
#define GPMAX 8
#define FRMAX 3
#define CORE_NUM 16 //remember to update this when you update other core numbers,
                    //minimum 3 cores, 1 for each type for global scheduler to not crash
#define PROB_LP 0.448 //probability of a LONG PROCESS
#define PROB_FR 0.344 //probability of an I/O heavy process
//Probability of GP Process is implicit : 1 - (PROB_LP + PROB_FR)
//if PROB_LP + PROB_FR evaluates close to 1.0, then efficiency will be very high (approching 100.00%)


// Mode enum for power-saving, balanced, and performance modes
typedef enum {POWER_SAVING, BALANCED, PERFORMANCE} Mode;

struct process_chain {
    struct process_chain* next;
    int compute_req; //-1 for I/O, the -1 exists so that in future you can actually have different types of I/O for 
                    //different negative numbers. Currently it is redundant.
};

// Process structure
typedef struct {
    int id;
    int arrival_time;
    int time_used;
    int assigned_queue; // LP, FR, GP, FB, LB
    int relinquishings;
    float interaction_ratio;
    struct process_chain* chain_ptr;
    int completion_time;
    int turnaround_time;
    int response_time;
    int waiting_time;
    int process_type;//only for debug use, not visible to scheduler
} Process;

// Core structure
typedef struct {
    int id;
    int core_type;
    int exec_time;
    int wasted_time;  //time wasted by process giving up time-slice partially
    int idle_time; //time where core doesn't have anything in its queue to process
    char core_type_string[20];
} Core;


struct qNode{
    Process* process;
    struct qNode* next;
};

typedef struct {
    struct qNode* front;
    struct qNode* back;
    int qSize;
} Queue;

// Scheduler and buffer queues
Process schedule[MAX_PROCESSES];
Queue lp_buffer, fr_buffer;
Queue global_queue;
Queue local_queue[CORE_NUM];

// Global Variables for mode and cores
Mode system_mode = BALANCED;
Core cores[CORE_NUM];
int fin_proc_num;
int LP_NUM = LPMAX;
int GP_NUM = GPMAX;
int FR_NUM = FRMAX;
int ACTIVE_CORES = CORE_NUM;

// Function prototypes
void initialize_cores();
void* global_scheduler(void* arg);
void set_mode(Mode new_mode);
void display_core_status();
void schedule_create();
void schedule_print(int finish);
int schedule_read(Process processes[MAX_PROCESSES]);
void populate_global(int num_proc);
int timeslice_mult(int core_type);
void* core_func(void* arg);
void clear_lp_buffer();
void clear_fr_buffer();
void* naive_scheduler(void* arg);
void* naive_core_func(void* arg);

int random_range(int min, int max){
   return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

void q_init(Queue* q) {
    q->back = NULL;
    q->front = NULL;
    q->qSize = 0;
}

void enqueue(Queue* q, Process* process) {
    q->qSize += 1;
    struct qNode* curr = (struct qNode*) malloc (sizeof(struct qNode));
    curr->next = NULL;
    curr->process = process;
    if(q->back == NULL) {
        q->front = q->back = curr;
    }
    else q->back->next = curr;
    q->back = curr;
    return;
}

Process* dequeue(Queue* q) {
    if (q->front == NULL) {
        // Queue is empty
        return NULL;
    }
    struct qNode* curr = q->front;
    Process* process = curr->process;

    if (curr->next == NULL) {
        // Single element in the queue
        q->front = q->back = NULL;
    } else {
        // More than one element
        q->front = q->front->next;
    }
    // Decrease queue size after successful dequeue
    q->qSize -= 1;
    free(curr); // Free the dequeued node's memory
    return process;
}

int main(int argc, char* argv[]) {
    //override NAIVE with CMDline Argument
    if(argc > 1) NAIVE = argv[1][0] - '0';
    if(NAIVE == 1) {
        NEW_SCHEDULE = 0;
        printf("\n<<---------------USING NAIVE SCHEDULING----------------->>\n");
    }
    else {
        //uses NEW_SCHEDULE from definition in top of code
        printf("\n<<---------------USING ADVANCED SCHEDULING----------------->>\n");
    }

    // Set the mode as set by MODE
    set_mode(MODE);

    // Initialize cores
    if(DEBUG) printf("Initializing Cores\n\n");
    initialize_cores();

    // Create Queues
    q_init(&lp_buffer);
    q_init(&fr_buffer);
    q_init(&global_queue);
    for(int i=0;i<CORE_NUM;i++) q_init(&local_queue[i]);

    // Create and Load the Process Table
    if(NEW_SCHEDULE == 1) schedule_create();

    unsigned long num_proc = schedule_read(schedule);
    if(DEBUG) printf("Initial Process Table\n");
    schedule_print(0);

    // Initialize Global Queue
    if(DEBUG) printf("\nInitializing Global Queue\n");
    populate_global(num_proc);

    // Create Threads to Simulate Cores Running in Parallel
    if(DEBUG) printf("\nCores Running Now\n");
    fin_proc_num = 0;
    pthread_t pCores[CORE_NUM], scheduler;
    for(unsigned long i=0;i<ACTIVE_CORES;i++) {
        if(NAIVE == 0) pthread_create(&pCores[i], NULL, &core_func, (void*) i);
        else pthread_create(&pCores[i], NULL, &naive_core_func, (void*) i);
    }

    // Run the Global Scheduler
    if(NAIVE == 0) pthread_create(&scheduler, NULL, &global_scheduler, (void*) num_proc);
    else pthread_create(&scheduler, NULL, &naive_scheduler, (void*) num_proc);
    
    // NOTE: Global Scheduler and All Cores Threads are dynamic and hence wait for future processes.
    // They are infinite loops, so to end the program, manual thread kill is required. So Don't pthread_join.

    // Wait and then Terminate
    usleep(1000000);
    for(int i=0;i<ACTIVE_CORES;i++) {
        pthread_kill(pCores[i], 0);
    }
    pthread_kill(scheduler, 0);

    //Display Final Statistics
    schedule_print(1);
    display_core_status();

    printf("<<-----------------------------END OF PROGRAM------------------------------->>\n");

    //Calling Naive Scheduler to Compare
    char* send_to_naive[] = {"main", "1"};
    if(argc < 2 && NAIVE == 0) main(2, send_to_naive);
    return 0;
}

void initialize_cores() {
    // Initialize LP, FR, and GP cores
    int i=0;
    for (int j = 0; j < LP_NUM; j++, i++) {
        cores[i].core_type = LP;
        cores[i].id = i;
        cores[i].exec_time = cores[i].wasted_time = 0;
        strcpy(cores[i].core_type_string, LONGPROCESS);
    }
    for (int j = 0; j < FR_NUM; j++, i++) {
        cores[i].core_type = FR;
        cores[i].id = i;
        cores[i].exec_time = cores[i].wasted_time = 0;
        strcpy(cores[i].core_type_string, FASTRESPONSE);
    }
    for (int j = 0; j < GP_NUM; j++, i++) {
        cores[i].core_type = GP;
        cores[i].id = i;
        cores[i].exec_time = cores[i].wasted_time = 0;
        strcpy(cores[i].core_type_string, GENERAL);
    }
}

void set_mode(Mode new_mode) {
    system_mode = new_mode;
    if (system_mode == POWER_SAVING) {
        if(DEBUG) printf("\nPOWER-SAVING mode: Reducing LP and GP core usage.\n");

        LP_NUM = LPMAX / 4;
        if(LP_NUM == 0) LP_NUM = 1;

        GP_NUM = GPMAX / 2;
        if(GP_NUM == 0) GP_NUM = 1;

        FR_NUM = FRMAX;
    } 
    else if (system_mode == BALANCED) {
        if(DEBUG) printf("\nBALANCED Mode: Even Core Distribution.\n");
        GP_NUM = GPMAX;
        FR_NUM = FRMAX * 0.75;
        LP_NUM = LPMAX * 0.5;
    } 
    else if (system_mode == PERFORMANCE) {
        if(DEBUG) printf("\nPERFORMANCE Mode: Activating all cores.\n");
        LP_NUM = LPMAX;
        GP_NUM = GPMAX;
        FR_NUM = FRMAX;
    }
    ACTIVE_CORES = LP_NUM + GP_NUM + FR_NUM;
    return;
}

void populate_global(int num_proc) {
    for(int i=0;i<num_proc;i++) {
        enqueue(&global_queue, &schedule[i]);
        //if(DEBUG) printf("Added Process %d to Global Queue\n", (&schedule[i])->id);
    }
}

void clear_lp_buffer() {
    Process* proc;
    while((proc = dequeue(&lp_buffer)) != NULL) {
            //remove from lp Buffer and add to LP queue with smallest number of proc in it
            int min_proc_queue_indx;
            int min_proc_queue_num = __INT_MAX__;
            Core* x_core;
            for(int corenum = 0; corenum < ACTIVE_CORES; corenum++) {
                x_core = &cores[corenum];
                if(x_core->core_type == LP && local_queue[x_core->id].qSize < min_proc_queue_num) {
                    min_proc_queue_num = local_queue[x_core->id].qSize;
                    min_proc_queue_indx = x_core->id;
                }
            }
            x_core = &cores[min_proc_queue_indx];
            proc->assigned_queue = LP;
            enqueue(&local_queue[min_proc_queue_indx], proc);
            if(DEBUG) printf("Process %d Re-Assigned to %s Core %d from LP-Buffer\n", proc->id, x_core->core_type_string, x_core->id);
    }
}

void clear_fr_buffer() {
    Process* proc;
    while((proc = dequeue(&fr_buffer)) != NULL) {
            //remove from fr Buffer and add to FR queue with smallest number of proc in it
            int min_proc_queue_indx;
            int min_proc_queue_num = __INT_MAX__;
            Core* x_core;
            for(int corenum = 0; corenum < ACTIVE_CORES; corenum++) {
                x_core = &cores[corenum];
                if(x_core->core_type == FR && local_queue[x_core->id].qSize < min_proc_queue_num) {
                    min_proc_queue_num = local_queue[x_core->id].qSize;
                    min_proc_queue_indx = x_core->id;
                }
            }
            x_core = &cores[min_proc_queue_indx];
            proc->assigned_queue = FR;
            enqueue(&local_queue[min_proc_queue_indx], proc);
            if(DEBUG) printf("Process %d Re-Assigned to %s Core %d from FR-Buffer\n", proc->id, x_core->core_type_string, x_core->id);
    }
}

void* global_scheduler(void* arg) {
    // Process global queue and assign tasks to appropriate cores/buffers
    unsigned long num_proc = (unsigned long) arg;
    int curr_time = 0;
    Process* proc;

    for(int i=0;i<num_proc;i++) {

        clear_lp_buffer();
        clear_fr_buffer();

        proc = dequeue(&global_queue);
        while(proc->arrival_time > curr_time) { //wait for arrival time
            curr_time += STD_TIMESLICE;
            usleep(STD_TIMESLICE);
        }
        int min_proc_queue_indx;
        int min_proc_queue_num = __INT_MAX__;
        Core* x_core;
        for(int corenum = 0; corenum < ACTIVE_CORES; corenum++) {
            x_core = &cores[corenum];
            if(x_core->core_type == GP && local_queue[x_core->id].qSize < min_proc_queue_num) {
                min_proc_queue_num = local_queue[x_core->id].qSize;
                min_proc_queue_indx = x_core->id;
            }
        }
        x_core = &cores[min_proc_queue_indx];
        proc->assigned_queue = GP;
        enqueue(&local_queue[min_proc_queue_indx], proc);
        if(DEBUG) printf("Process %d Assigned to %s Core %d from Global Queue\n", proc->id, x_core->core_type_string, x_core->id);
    }
    //after the initial processes are assigned, this handles any changes untill all processes terminate
    while(1) {
        clear_lp_buffer();
        clear_fr_buffer();
    }
}

int timeslice_mult(int core_type) {
    if(core_type == LP) return LP_TIMESLICE_MULT;
    if(core_type == GP) return GP_TIMESLICE_MULT;
    if(core_type == FR) return FR_TIMESLICE_MULT;

    printf("Invalid Core Type: %d\n", core_type);
    return 0;
}

void* core_func(void* arg) {

    unsigned long id = (unsigned long) arg;
    Core* core = &cores[id];
    int multiplier = timeslice_mult(core->core_type);
    int time_slice = STD_TIMESLICE * multiplier;
    int compute;
    if(core->core_type == LP) compute = LP_COMPUTE * multiplier;
    else if(core->core_type == FR) compute = FR_COMPUTE * multiplier;
    else compute = GP_COMPUTE * multiplier;
    int curr_time = 0;
    
    if(DEBUG) printf("Core::ID = %d, Type = %s\n", core->id, core->core_type_string);

    while(1) {
        while(local_queue[core->id].qSize <= 0) {
            //waiting for process to come to local queue
            core->idle_time += time_slice;
            curr_time += time_slice;
            usleep(time_slice);
        };  

        curr_time += time_slice; //tracking time
        Process* currproc = dequeue(&local_queue[core->id]);
        if(currproc == NULL) {
            if(DEBUG) printf("<----------PROCESS QUEUE IS EMPTY FOR %s CORE id: %d------------>\n", core->core_type_string, core->id);
            continue;
        }

        if(currproc->response_time == -1) currproc->response_time = curr_time - currproc->arrival_time;   //setting response time
        struct process_chain* ptr = currproc->chain_ptr;

        currproc->time_used += time_slice;
        core->exec_time += time_slice; //for Core statistics tracking

        if(ptr->compute_req == -1) {
            // I/O request
            currproc->relinquishings += 1;
            if(currproc->relinquishings!=0) currproc->interaction_ratio = ((float)currproc->relinquishings / (float) currproc->time_used) * 100000;
            currproc->chain_ptr = currproc->chain_ptr->next;
            // I/O means processor doesn't actually take any time
            curr_time -= time_slice;
            currproc->time_used -= time_slice;
            core->exec_time -= time_slice;
        }
        else if(ptr->compute_req > compute) {
            //required compute more than compute of timeslice
            ptr->compute_req -= compute;
        }
        else {
            int time_used_in_slice = time_slice * (ptr->compute_req / compute); 
            core->wasted_time += (time_slice - time_used_in_slice);
            currproc->chain_ptr = currproc->chain_ptr->next;
        }

        int finish = 0;
        int change_core = 0;
        if(currproc->chain_ptr == NULL) {
            //process finished
            if(DEBUG) printf("Process %d has FINISHED SUCCESSFULLY. ORDER %d\n", currproc->id, ++fin_proc_num);
            currproc->completion_time = curr_time;
            currproc->turnaround_time = currproc->completion_time - currproc->arrival_time;
            currproc->waiting_time = currproc->turnaround_time - currproc->time_used;
            finish = 1;
        }
        if(core->core_type == GP && finish == 0) {
            if(currproc->time_used > LONG_PROC_THRESH) {
                //SWITCH THIS PROCESS TO LP Core
                change_core = 1;
                if(DEBUG) printf("Process %d Removed from %s Core %d - Added to LP Buffer\n", currproc->id, core->core_type_string, core->id);
                enqueue(&lp_buffer, currproc);
            }

            else if(currproc->interaction_ratio > INTER_RATIO_THRESH) {
                //SWITCH THIS PROCESS TO FR Core
                change_core = 1;
                if(DEBUG) printf("Process %d Removed from %s Core %d - Added to FR Buffer\n", currproc->id, core->core_type_string, core->id);
                enqueue(&fr_buffer, currproc);
            }
        }
        if(finish == 0 && change_core == 0) {
            enqueue(&local_queue[core->id], currproc);
        }
    }
}

void schedule_create() {
    FILE *fp = fopen(SCHEDULE, "w+");
    if (!fp) {
        perror("File open failed");
        exit(EXIT_FAILURE);
    }
    
    //id uniformly increasing.
    //the arrival times must be uniformly increasing as we traverse the array. arrival_time in the range [0, 500], 
    srand((unsigned)time(NULL));
    int arrival_time = 0;

    // Write header for clarity
    fprintf(fp, "ID\tAT\tUsed\tCore\tRelinq\tRatio\tPTR\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        Process process;
        // ID and uniformly increasing arrival time
        process.id = i;
        process.arrival_time = arrival_time;
        arrival_time += (rand() % ARRIVAL_TIME_MAX_INCREMENT) + 1; // Increase by a random amount

        process.time_used = 0;
        process.assigned_queue = GL;
        process.relinquishings = 0;
        process.interaction_ratio = 0.00; //relinquishings divided by time_used so far * 100000

        //generate process chain for the process
        //3 types of process, distribution depending on probability

        int process_type;
        float process_type_prob = rand()/(float)RAND_MAX;
        if(process_type_prob < PROB_LP) process_type = LP;
        else if(process_type_prob < PROB_LP + PROB_FR) process_type = FR;
        else process_type = GP;

        int steps = ( rand() % CHAIN_MAX_ITEMS ) + 1;
        struct process_chain* head = NULL;
        struct process_chain* ptr = NULL;
        bool exec_or_io = true;

        for(int i=0;i<steps;i++) {
            
            struct process_chain* curr;
            curr = (struct process_chain*) malloc (sizeof(struct process_chain));
            curr->next = NULL;

            int compute_req;
            if(exec_or_io) {
                if(process_type == LP) {
                    process.process_type = LP;
                    compute_req = random_range(LP_COMPUTE_LOWERBOUND, LP_COMPUTE_UPPERBOUND);
                }
                else if(process_type == FR) {
                    process.process_type = FR;
                    compute_req = random_range(0, FR_COMPUTE_UPPERBOUND);
                }
                else {
                    process.process_type = GP;
                    compute_req = rand() % MAX_COMPUTE;
                }
                curr->compute_req = compute_req;
            }
            else curr->compute_req = -1;

            exec_or_io = !exec_or_io;
            if(head == NULL) {
                head = ptr = curr;
            }
            else {
                ptr->next = curr;
                ptr = curr;
            }
        }

        process.chain_ptr = head;
        process.completion_time = -1;
        process.turnaround_time = -1;
        process.response_time = -1;
        process.waiting_time = -1;

        // Write process to file
        fprintf(fp, "%d\t%d\t%d\t%d\t%d\t%.2f\t%p\t%d\t%d\t%d\t%d\t%d\n", process.id, process.arrival_time, process.time_used, process.assigned_queue,
                process.relinquishings, process.interaction_ratio, process.chain_ptr, process.completion_time, process.turnaround_time,
                process.response_time, process.waiting_time, process.process_type);

        ptr = head;
        for(int i=0;i<steps;i++) {
            fprintf(fp, "%d\t%p\n", ptr->compute_req, ptr->next);
            ptr = ptr->next;
        }
    }
    fclose(fp);
    printf("Process schedule created in '%s'.\n", SCHEDULE);
}

int schedule_read(Process processes[MAX_PROCESSES]) {
    FILE *fp = fopen(SCHEDULE, "r");
    if (!fp) {
        perror("File open failed");
        exit(EXIT_FAILURE);
    }
    struct process_chain* dummy;
    char line[256];
    char line2[256];
    // Skip the header line
    fgets(line, sizeof(line), fp);

    int count = 0;
    while (fgets(line, sizeof(line), fp) && count < MAX_PROCESSES) {
        Process process;
        if (sscanf(line, "%d\t%d\t%d\t%d\t%d\t%f\t%p\t%d\t%d\t%d\t%d\t%d",
                   &process.id,
                   &process.arrival_time,
                   &process.time_used,
                   &process.assigned_queue,
                   &process.relinquishings,
                   &process.interaction_ratio,
                   &dummy,
                   &process.completion_time, 
                   &process.turnaround_time,
                   &process.response_time,
                   &process.waiting_time,
                   &process.process_type) == 12) {

            struct process_chain* head = NULL;
            struct process_chain* ptr = NULL;
            while(1) {
                fgets(line2, sizeof(line2), fp);
                struct process_chain* curr = (struct process_chain*) malloc (sizeof(struct process_chain));
                sscanf(line2, "%d\t%p", &curr->compute_req, &curr->next);
                if(head == NULL) head = ptr = curr;
                else {
                    ptr->next = curr;
                    ptr = curr;
                }
                if(curr->next == NULL) break;
            }
            process.chain_ptr = head;
            processes[count++] = process;
        } else {
            fprintf(stderr, "Error parsing line: %s", line);
        }
    }

    fclose(fp);
    return count; // Return the number of processes read
}

void schedule_print(int finish) {
    __uint64_t cumulative_TAT = 0, cumulative_RT = 0, cumulative_WT = 0;
    int core_matches = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        cumulative_TAT += schedule[i].turnaround_time;
        cumulative_RT += schedule[i].response_time;
        cumulative_WT += schedule[i].waiting_time;

        char core_type[20] = {0};
        char proc_type[20] = {0};
        int assigned_queue = schedule[i].assigned_queue;
        int process_type = schedule[i].process_type;
        if(assigned_queue == LP) strcpy(core_type, LONGPROCESS);
        else if(assigned_queue == GP) strcpy(core_type, GENERAL);
        else if(assigned_queue == FR) strcpy(core_type, FASTRESPONSE);
        
        if(process_type == LP) strcpy(proc_type, LONGPROC);
        else if(process_type == GP) strcpy(proc_type, GENLPROC);
        else if(process_type == FR) strcpy(proc_type, FASTPROC);
        
        if(assigned_queue == process_type || assigned_queue == GP) core_matches += 1;

        if(DEBUG) {printf("ID: %4d, Arrival Time: %5d, Burst Time: %5d, CT: %5d, TAT: %5d, RT: %5d, WT: %5d,  Queue: %12s,  P_TYPE: %9s,  RLQ: %2d, INTR: %.6f\t ",
               schedule[i].id,
               schedule[i].arrival_time/1000,
               schedule[i].time_used/1000,
               schedule[i].completion_time/1000,
               schedule[i].turnaround_time/1000,
               schedule[i].response_time/1000,
               schedule[i].waiting_time/1000,
               core_type,
               proc_type,
               schedule[i].relinquishings,
               schedule[i].interaction_ratio);
               
        struct process_chain* ptr = schedule[i].chain_ptr;
        printf("PROCESS CHAIN: ");
        while(ptr) {
            printf("-->%d", ptr->compute_req);
            ptr = ptr->next;
        }
        printf("\n");}
    }
    if(finish == 1) {
        printf("\nAverage TAT: %ld\tAverage RT: %ld\tAverage WT: %ld\n",
                cumulative_TAT/(MAX_PROCESSES*1000),
                cumulative_RT/(MAX_PROCESSES*1000),
                cumulative_WT/(MAX_PROCESSES*1000));
        printf("Core Match Efficiency: %2.1f%%\n\n", (core_matches / (float)MAX_PROCESSES) * 100);
    }
}

void display_core_status() {
    int avg_waste = 0, avg_idle = 0;
    printf("Core Statistics:\n");
    for (int i = 0; i < ACTIVE_CORES; i++) {
        int id = cores[i].id, exec_time = cores[i].exec_time/1000, wasted_time = cores[i].wasted_time/1000, idle = (cores[i].idle_time)/1000;
        avg_waste += wasted_time;
        avg_idle += idle;
        printf(" Core %d:\tCore Type: %s\tTotal Exec Time = %d ms,\tTotal Wasted Time = %d ms,\tTotal Idle Time: %d ms\n",
            id, cores[i].core_type_string, exec_time, wasted_time, idle);
    }

    printf("Power Usage: %d Watts", LP_COMPUTE*LP_NUM + FR_COMPUTE*FR_NUM + GP_COMPUTE*FR_NUM);
    printf("\nAverage Wasted Time: %d ms,\tAverage Idle Time: %d ms\n", avg_waste/ACTIVE_CORES, avg_idle/ACTIVE_CORES);
}

void* naive_scheduler(void* arg) {
    unsigned long num_proc = (unsigned long) arg;
    int curr_time = 0;
    int corenum = 0;
    for(int i=0;i<num_proc;i++) {
        Process* proc = dequeue(&global_queue);
        Core* core = &cores[corenum];

        while(proc->arrival_time > curr_time) { //wait for arrival time
            curr_time += STD_TIMESLICE;
            usleep(STD_TIMESLICE);
        }

        proc->assigned_queue = core->core_type;
        enqueue(&local_queue[corenum], proc);
        if(DEBUG) printf("Process %d Assigned to %s Core %d\n", proc->id, core->core_type_string, core->id);
        corenum = (corenum + 1) % ACTIVE_CORES;
    }
}

void* naive_core_func(void* arg) {

    unsigned long id = (unsigned long) arg;
    Core* core = &cores[id];
    int multiplier = timeslice_mult(core->core_type);
    int time_slice = STD_TIMESLICE * multiplier;
    int compute;
    if(core->core_type == LP) compute = LP_COMPUTE * multiplier;
    else if(core->core_type == FR) compute = FR_COMPUTE * multiplier;
    else compute = GP_COMPUTE * multiplier;
    int curr_time = 0;
    
    if(DEBUG) printf("Core::ID = %d, Type = %s\n", core->id, core->core_type_string);

    while(1) {
        while(local_queue[core->id].qSize <= 0) {
            //waiting for process to come to local queue
            core->idle_time += time_slice;
            curr_time += time_slice;
            usleep(time_slice);
        };  

        curr_time += time_slice; //tracking time
        Process* currproc = dequeue(&local_queue[core->id]);
        if(currproc == NULL) {
            if(DEBUG) printf("<----------PROCESS QUEUE IS EMPTY FOR %s CORE id: %d------------>\n", core->core_type_string, core->id);
            continue;
        }

        if(currproc->response_time == -1) currproc->response_time = curr_time - currproc->arrival_time;   //setting response time
        struct process_chain* ptr = currproc->chain_ptr;

        currproc->time_used += time_slice;
        core->exec_time += time_slice; //for Core statistics tracking

        if(ptr->compute_req == -1) {
            // I/O request
            currproc->relinquishings += 1;
            if(currproc->relinquishings!=0) currproc->interaction_ratio = ((float)currproc->relinquishings / (float) currproc->time_used) * 100000;
            currproc->chain_ptr = currproc->chain_ptr->next;
            // I/O means processor doesn't actually take any time
            curr_time -= time_slice;
            currproc->time_used -= time_slice;
            core->exec_time -= time_slice;
        }
        else if(ptr->compute_req > compute) {
            //required compute more than compute of timeslice
            ptr->compute_req -= compute;
        }
        else {
            int time_used_in_slice = time_slice * (ptr->compute_req / compute); 
            core->wasted_time += (time_slice - time_used_in_slice);
            currproc->chain_ptr = currproc->chain_ptr->next;
        }
        if(currproc->chain_ptr == NULL) {
            //process finished
            if(DEBUG) printf("Process %d has FINISHED SUCCESSFULLY. ORDER %d\n", currproc->id, ++fin_proc_num);
            currproc->completion_time = curr_time;
            currproc->turnaround_time = currproc->completion_time - currproc->arrival_time;
            currproc->waiting_time = currproc->turnaround_time - currproc->time_used;
        }
        else {
            enqueue(&local_queue[core->id], currproc);
        }
    }
}

//END