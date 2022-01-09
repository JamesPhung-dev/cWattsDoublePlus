#define _GNU_SOURCE

#include <math.h>
#include <papi.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_EVENTS 6
#define NO_OF_CPUS 4
int MAXWORKLOADS = 12;

void cal_workload();
void * launcher(void *);

int main(int argc, char ** argv)
{
    // Initialise variables
    pthread_t worker_id[MAXWORKLOADS];
    pthread_attr_t attr;
    // cpu_set_t cpus;

    PAPI_option_t opt;
    int EventSet = PAPI_NULL;
    // long long elapsed;
    long long values[NUM_EVENTS];
    FILE * temp_output[4];
    int temps[4];
    char filename[256];
    FILE * data_out = NULL;

    if (argc == 2)
    {
        // Ensure the output file is blank
        sscanf(argv[1], "%s", filename);
        data_out = fopen(filename, "w");
        fclose(data_out);
    }
    else if (argc > 2)
    {
        puts("Too many arguments!");
        exit(1);
    }
    else
    {
        puts("Missing filename");
        exit(1);
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_thread_init(pthread_self);
    if ((PAPI_set_domain(PAPI_DOM_ALL)) != PAPI_OK)
    {
        fprintf(stderr, "PAPI_set_domain - FAILED\n");
        exit(1);
    }
    PAPI_create_eventset(&EventSet);
    PAPI_add_event(EventSet, PAPI_TOT_CYC);
    PAPI_add_event(EventSet, PAPI_REF_CYC);
    int native = 0;
    PAPI_event_name_to_code("UOPS_ISSUED:t=0", &native);
    PAPI_add_event(EventSet, native);
    // PAPI_add_event(EventSet, PAPI_L3_LDM);
    PAPI_event_name_to_code("LLC-LOAD-MISSES", &native); // CHECK THIS!
    PAPI_add_event(EventSet, native);
    PAPI_add_event(EventSet, PAPI_TLB_DM); // CHECK THIS!
    PAPI_add_event(EventSet, PAPI_TLB_IM); // CHECK THIS!
    // PAPI_add_event(EventSet, PAPI_LD_INS);
    // PAPI_add_event(EventSet, PAPI_SR_INS);
    // PAPI_add_event(EventSet, PAPI_MEM_WCY);
    // PAPI_add_event(EventSet, PAPI_BR_MSP);
    memset(&opt, 0x0, sizeof(PAPI_option_t));
    opt.inherit.inherit = PAPI_INHERIT_ALL;
    opt.inherit.eventset = EventSet;
    PAPI_set_opt(PAPI_INHERIT, &opt);

    // Start workloads
    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "fork: Oh no!\n");
        exit(1);
    }
    if (pid == 0)
    {
        char * core[MAXWORKLOADS];

        // run workloads here
        for (int k = 0; k < MAXWORKLOADS; k++)
        {
            core[k] = (char *)malloc(sizeof(char)*2);
            sprintf(core[k], "%d", k);
            // CPU_ZERO(&cpus);
            // CPU_SET(k % NO_OF_CPUS, &cpus);
            // pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
            pthread_create(&worker_id[k], &attr, launcher, core[k]);
        }
        pthread_exit(NULL);
    }

    // Attach event counters to child process
    if (PAPI_attach(EventSet, pid) != PAPI_OK)
    {
        fprintf(stderr, "PAPI_attach: Oh no!\n");
        exit(1);
    }

    // Open core temperature streams
    temp_output[0] = fopen("/sys/devices/platform/coretemp.0/temp2_input", "r");
    temp_output[1] = fopen("/sys/devices/platform/coretemp.0/temp3_input", "r");
    temp_output[2] = fopen("/sys/devices/platform/coretemp.0/temp4_input", "r");
    temp_output[3] = fopen("/sys/devices/platform/coretemp.0/temp5_input", "r");

    // Loop every second
    while(1)
    {
        // Get CPU counters at start of one second interval
        for (int k = 0; k < NUM_EVENTS; k++)
        {
            values[k] = 0;
        }

        // PAPI_reset(EventSet);
        if (PAPI_start(EventSet) != PAPI_OK)
        {
            fprintf(stderr, "PAPI_start_counters - FAILED\n");
            exit(1);
        }
        // elapsed = PAPI_get_real_cyc();

        // Pause for one second
        sleep(1);

        // Get CPU counters at end of one second interval
        // elapsed = PAPI_get_real_cyc() - elapsed;
        if (PAPI_read(EventSet, values) != PAPI_OK)
        {
            fprintf(stderr, "PAPI_read_counters - FAILED\n");
            exit(1);
        }
        if (PAPI_stop(EventSet, values) != PAPI_OK)
        {
            fprintf(stderr, "PAPI_stoped_counters - FAILED\n");
            exit(1);
        }
        fscanf(temp_output[0], "%d", &temps[0]);
        fscanf(temp_output[1], "%d", &temps[1]);
        fscanf(temp_output[2], "%d", &temps[2]);
        fscanf(temp_output[3], "%d", &temps[3]);

        for (int k = 0; k < 4; k++)
        {
            fseek(temp_output[k], 0, SEEK_END);
            fseek(temp_output[k], 0, SEEK_SET);
            temps[k] /= 1000;
        }

        data_out = fopen(filename, "a");
        char output_row[256];
        sprintf(output_row, "%d,%lld,%lld,%lld,%lld,%lld,%lld,%d,%d,%d,%d\n",
                MAXWORKLOADS, values[0], values[1], values[2], values[3], values[4], values[5], temps[0], temps[1], temps[2], temps[3]);
        fprintf(data_out, "%s", output_row);
        fclose(data_out);
    }
    exit(0);
}

void * launcher(void * core)
{
    // printf("%s\n", (char *)core);
    // execlp("taskset", "taskset", "-c", core, "stress", "-c", "256", NULL);
    // puts("execlp: Oh no!");
    cal_workload();
    exit(1);
}

void cal_workload()
{
    double dummy1 = -139872;
    double dummy2 = 231.312;
    // long dummy1 = -139872;
    // long dummy2 = 132;
    // unsigned long k = 0;
    // char * long_word = (char *)malloc(32768*sizeof(char));
    // char * duplicate = (char *)malloc(32768*sizeof(char));

    while (1)
    {
        dummy1 += tanl(1 + dummy1 / 1.1);
        dummy2 -= tanl(1 + dummy2 / 1.2);
        dummy1 = dummy2 / dummy1;

        /*dummy1 += 563;
        dummy2 -= 32;
        dummy1 = dummy2 * dummy1;*/
    }
    /*while (1)
    {
        long_word[k] = 'A';
        long_word[k+1] = '\0';
        if (k < 32766)
        {
            k+= 1;
        }
        else
        {
            k = 0;
        }
        strcpy(duplicate, long_word);
    }*/
}
