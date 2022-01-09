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

#define MAX_CPUS 4
#define NUM_EVENTS 6

int main(int argc, char ** argv)
{
    // Initialise variables
    double b = 1.156598254001463;
    double c = -0.915825820677378;
    double d = 0.273913728795395;
    double e = 0.00000025;
    double f = 0.00000025;
    double g = 0.5;
    double h = 0;
    double i = 25;

    /*
    double c = 0.376200997345436;
    double d = 0.002160104193748;
    double e = 0.0000002;
    double f = 0.0000002;
    double g = 0.6;
    double h = 0;
    double i = 22; */

    //PAPI_option_t opt;
    // int EventSet = PAPI_NULL;
    int * EventSet = NULL;
    double * sub_power = NULL;
    double power = 0;
    double coverage_ratio = 0;
    //long long values[NUM_EVENTS];
    long long ** values = NULL;
    FILE * temp_output[4];
    int temps[4];
    double excess_temp = 0;
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

    if ((PAPI_set_domain(PAPI_DOM_ALL)) != PAPI_OK) // CHECK AND REMOVE THIS!
    {
        fprintf(stderr, "PAPI_set_domain - FAILED\n");
        exit(1);
    }
    int native1 = 0;
    PAPI_event_name_to_code("UOPS_ISSUED:t=0", &native1);
    //int native2 = 0;
    //PAPI_event_name_to_code("LLC-LOAD-MISSES", &native2);
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
        PAPI_add_event(EventSet[k], PAPI_L3_LDM);
        PAPI_add_event(EventSet[k], PAPI_TLB_DM);
        PAPI_add_event(EventSet[k], PAPI_TLB_IM);
        //memset(&opt, 0x0, sizeof(PAPI_option_t));
        //opt.inherit.inherit = PAPI_INHERIT_ALL;
        //opt.inherit.eventset = EventSet[k];
        //PAPI_set_opt(PAPI_INHERIT, &opt);
    }

    values = (long long **)malloc(sizeof(long long *)*no_of_pids);
    sub_power = (double *)malloc(sizeof(double)*no_of_pids);

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

        excess_temp = 0;

        for (int k = 0; k < 4; k++)
        {
            fseek(temp_output[k], 0, SEEK_END);
            fseek(temp_output[k], 0, SEEK_SET);
            temps[k] /= (double)1000;
            excess_temp += temps[k];
        }
        excess_temp /= (double)4;
        if (excess_temp >= 60)
        {
            excess_temp -= 60;
        }
        else
        {
            excess_temp = 0;
        }
        power = 0;
        coverage_ratio = 0;
        double sub_freq = 0;
        double avg_freq = 0;
        for (int k = 0; k < no_of_pids; k++)
        {
            sub_freq = (double)values[k][0]/(values[k][1] * 10); // Frequency in GHz
            avg_freq += sub_freq;
            sub_power[k] = (double)values[k][2] / 1000000000 * (sub_freq * b + pow(sub_freq, 1.5) * c + pow(sub_freq, 2) * d)
                         + (double)values[k][3] * e
                         + ((double)values[k][4] + (double)values[k][5]) * f ; // Estimate power use of a process
            coverage_ratio += values[k][1];
            printf("Estimated process power use: %lf W\n", sub_power[k]);
            power += sub_power[k];
        }
        power = power * 400000000 / (double)coverage_ratio;
        avg_freq /= (double)no_of_pids;
        printf("frequency: %lf\n", avg_freq);
        power += (double)avg_freq * g + (double)pow(avg_freq, 2) * h;
        power += (double)i;
        power += (double)excess_temp * 1.1 / 5;
        // power = pow(power, 1.00); // SCALING FACTOR (CHECK THIS)!
        printf("Estimated total power use: %lf W\n", power);
        data_out = fopen(filename, "a");
        fprintf(data_out, "%lf\n", power);
        fclose(data_out);
        fflush(stdout);
    }
    exit(0);
}
