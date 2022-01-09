#define _GNU_SOURCE

#include <math.h>
#include <papi.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_EVENTS 6

int main(int argc, char ** argv)
{
    // PAPI_option_t opt;
    // int EventSet = PAPI_NULL;
    int * EventSet = NULL;
    // long long values[NUM_EVENTS];
    long long ** values = NULL;
    long long * params = NULL;
    FILE * temp_output[4];
    int temps[4];
    char filename[256];
    FILE * data_out = NULL;
    pid_t pid = 0;

    if (argc > 2)
    {
        // Ensure the output file is blank
        sscanf(argv[1], "%s", filename);
        data_out = fopen(filename, "w");
        fclose(data_out);
    }
    else
    {
        puts("Missing arguments!");
        exit(1);
    }

    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_thread_init(pthread_self);

    if ((PAPI_set_domain(PAPI_DOM_ALL)) != PAPI_OK)
    {
        fprintf(stderr, "PAPI_set_domain - FAILED\n");
        exit(1);
    }
    int native1 = 0;
    PAPI_event_name_to_code("UOPS_ISSUED:t=0", &native1);
    int native2 = 0;
    PAPI_event_name_to_code("LLC-LOAD-MISSES", &native2);
    int no_of_pids = 0;
    for (int k = 2; k < argc; k++)
    {
        if (strcmp(argv[k], "-"))
        {
            no_of_pids++;
        }
        else
        {
            no_of_pids += atoi(argv[k+2]) - atoi(argv[k+1]) + 1;
            break;
        }
    }
    EventSet = (int *)malloc(sizeof(int)*no_of_pids);
    for (int k = 0; k < no_of_pids; k++)
    {
        EventSet[k] = PAPI_NULL;
        PAPI_create_eventset(&(EventSet[k]));
        PAPI_add_event(EventSet[k], PAPI_TOT_CYC);
        PAPI_add_event(EventSet[k], PAPI_REF_CYC);
        PAPI_add_event(EventSet[k], native1);
        //PAPI_add_event(EventSet[k], native2);
        PAPI_add_event(EventSet[k], PAPI_L3_LDM);
        PAPI_add_event(EventSet[k], PAPI_TLB_DM);
        PAPI_add_event(EventSet[k], PAPI_TLB_IM);
        //printf("The return value is %d.\n", temp);
        //memset(&opt, 0x0, sizeof(PAPI_option_t));
        //opt.inherit.inherit = PAPI_INHERIT_ALL;
        //opt.inherit.eventset = EventSet[k];
        //PAPI_set_opt(PAPI_INHERIT, &opt);
    }

    values = (long long **)malloc(sizeof(long long *)*no_of_pids);

    // Attach event counters to workloads
    int flag = -1;
    for (int k = 0; k < no_of_pids; k++)
    {
        if ((flag == -1) && strcmp(argv[k+2], "-"))
        {
            pid = atoi(argv[k+2]);
        }
        else
        {
            if (flag == -1)
            {
                flag = k + 2;
            }
            pid = atoi(argv[flag+1]) + k + 2 - flag;
        }
        if (PAPI_attach(EventSet[k], pid) != PAPI_OK)
        {
            fprintf(stderr, "PAPI_attach: Oh no!\n");
            exit(1);
        }
        values[k] = (long long *)malloc(sizeof(long long)*NUM_EVENTS);
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
        for (int l = 0; l < NUM_EVENTS; l++)
        {
            for (int k = 0; k < no_of_pids; k++)
            {
                values[k][l] = 0;
            }
        }

        for (int k = 0; k < no_of_pids; k++)
        {
            if (PAPI_start(EventSet[k]) != PAPI_OK)
            {
                fprintf(stderr, "PAPI_start_counters - FAILED\n");
                exit(1);
            }
        }

        // Pause for one second
        sleep(1);

        // Get CPU counters at end of one second interval
        for (int k = 0; k < no_of_pids; k++)
        {
            if (PAPI_read(EventSet[k], values[k]) != PAPI_OK)
            {
                fprintf(stderr, "PAPI_read_counters - FAILED\n");
                exit(1);
            }
            if (PAPI_stop(EventSet[k], values[k]) != PAPI_OK)
            {
                fprintf(stderr, "PAPI_stoped_counters - FAILED\n");
                exit(1);
            }
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

        params = (long long *)malloc(sizeof(long long)*NUM_EVENTS);

        for (int k = 0; k < NUM_EVENTS; k++)
        {
            params[k] = 0;
            for (int l = 0; l < no_of_pids; l++)
            {
                params[k] += values[l][k];
            }
        }

        data_out = fopen(filename, "a");
        char output_row[256];
        sprintf(output_row, "%lld,%lld,%lld,%lld,%lld,%lld,%d,%d,%d,%d\n",
                params[0], params[1], params[2], params[3], params[4], params[5], temps[0], temps[1], temps[2], temps[3]);
        fprintf(data_out, "%s", output_row);
        fclose(data_out);
    }
    exit(0);
}
