#define _GNU_SOURCE

#include <errno.h>
#include <math.h>
#include <papi.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define NUM_EVENTS 6
#define NO_OF_CPUS 4
int MAXWORKLOADS = 1;

void cal_workload();
void * launcher(void *);
void eratosthenes(void *);

int main(int argc, char ** argv)
{
    // Initialise variables
    double a = 0.454713646518109;
    double b = 2.142116775746064;
    double c = -0.015294006345865;
    double d = 0.018383102653852;
    double e = 26.343319335166068;

    pthread_t worker_id[MAXWORKLOADS];
    pthread_attr_t attr;
    //cpu_set_t cpus;

    PAPI_option_t opt;
    int EventSet = PAPI_NULL;
    long long elapsed;
    double power = 0;
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
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_thread_init(pthread_self);
    /*if ((PAPI_set_domain(PAPI_DOM_ALL)) != PAPI_OK)
    {
        fprintf(stderr, "PAPI_set_domain - FAILED\n");
        exit(1);
    }*/
    PAPI_create_eventset(&EventSet);
    PAPI_add_event(EventSet, PAPI_TOT_CYC);
    PAPI_add_event(EventSet, PAPI_REF_CYC);
    int native = 0;
    PAPI_event_name_to_code("UOPS_ISSUED:t=0", &native);
    PAPI_add_event(EventSet, native);
    PAPI_event_name_to_code("LLC-LOAD-MISSES", &native); // CHECK THIS!
    PAPI_add_event(EventSet, native);
    PAPI_add_event(EventSet, PAPI_TLB_DM); // CHECK THIS!
    PAPI_add_event(EventSet, PAPI_TLB_IM); // CHECK THIS!
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
        int * work_no[MAXWORKLOADS];

        // run workloads here
        for (int k = 0; k < MAXWORKLOADS; k++)
        {
            work_no[k] = (int *)malloc(sizeof(int));
            *(work_no[k]) = k;
            // CPU_ZERO(&cpus);
            // CPU_SET(k % NO_OF_CPUS, &cpus);
            // pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
            pthread_create(&worker_id[k], &attr, launcher, work_no[k]);
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
        elapsed = PAPI_get_real_cyc();

        // Pause for one second
        sleep(1);

        // Get CPU counters at end of one second interval
        elapsed = PAPI_get_real_cyc() - elapsed;
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

        printf("Cycles elapsed: %lld\n", elapsed);
        printf("Total unhalted cycles: %lld\n", values[0]);
        printf("Total reference cycles: %lld\n", values[1]);
        printf("Total micro-operations issued: %lld\n", values[2]);
        printf("Total LLC read misses: %lld\n", values[3]);
        printf("Total TLB data misses: %lld\n", values[4]);
        printf("Total TLB instruction misses: %lld\n", values[5]);

        printf("Core 0 temperature: %d\n", temps[0]);
        printf("Core 1 temperature: %d\n", temps[1]);
        printf("Core 2 temperature: %d\n", temps[2]);
        printf("Core 3 temperature: %d\n", temps[3]);
        double freq = (double)values[0]/(values[1] * 10); // Frequency in GHz
        // double temp_avg = ((double)(temps[0]+ temps[1] + temps[2] + temps[3]))/4;
        power = freq * a + pow(freq, 2) * b + values[2] / 1000000000 * c + pow(values[2] / 1000000000, 2) * d + e; // Estimate overall power use
        printf("Estimated power use: %lf W\n", power);
        data_out = fopen(filename, "a");
        fprintf(data_out, "%lf\n", power);
        fclose(data_out);
        fflush(stdout);
    }
    exit(0);
}

void * launcher(void * work_no)
{
    // printf("%s\n", (char *)core);
    // execlp("./eratosthenes", "./eratosthenes", "1048576", NULL);
    // puts("execlp: Oh no!");
    cal_workload();
    exit(1);
}

void cal_workload()
{
    double dummy1 = -139872;
    long dummy2 = -139872;
    unsigned long k = 0;
    char * long_word = (char *)malloc(32768*sizeof(char));
    FILE * phoney = fopen("/dev/null", "w");

    while (1)
    {
        dummy1 += tanl(1 + dummy1 / 1.1);
        if (dummy1 < 0)
        {
            dummy1 *= -1;
        }
        if (dummy1 < 1)
        {
            dummy1 += 2;
        }
        dummy1 = sqrt(dummy1);
        dummy2 += 563;
        dummy1 += dummy2;
        fprintf(phoney, "%f\n", dummy1);
    // }
    // while (1)
    // {
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
        fprintf(phoney, "%s\n", long_word);
        for (int n = 1; n < 1000; n++)
        {
            fprintf(phoney, "I am a duck!\n");
        }
    }
}
