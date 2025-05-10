#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "../ioctl_toggle.h"

volatile int toggle_change_count = 0;
int max_toggles = 10;
int debugMode = 0;
int fd;
struct timespec start_hr, end_hr;
time_t start_time_print, end_time_print;

void simulate_toggle_changes() {
    int value;
    srand(time(NULL));

    while (toggle_change_count < max_toggles) {
        value = rand() % 2;
        toggle_change_count++;
        if (ioctl(fd, IOCTL_TOGGLE_SET, &value) < 0) {
            perror("Error setting toggle value");
        } else if (debugMode) {
            printf("Simulated Toggle change: set to %d\n", value);
        }
    }
}

void run_test(int current_rep, int total_reps) {
    toggle_change_count = 0;

    fd = open("/dev/toggle", O_RDWR);
    if (fd < 0) {
        perror("Error opening device file");
        return;
    }

    printf("This is a test for the C kernel module\n");
    printf("Starting test %d out of %d\n", current_rep, total_reps);
    printf("\n");

    time(&start_time_print);
    printf("Start time: %s", ctime(&start_time_print));
    printf("Start Unix time: %ld\n", start_time_print);

    clock_gettime(CLOCK_MONOTONIC, &start_hr);

    if (debugMode)
        printf("Starting Toggle simulation in DEBUG mode...\n");

    simulate_toggle_changes();

    time(&end_time_print);
    printf("End time: %s", ctime(&end_time_print));
    printf("End Unix time: %ld\n", end_time_print);

    clock_gettime(CLOCK_MONOTONIC, &end_hr);
    double elapsed = (end_hr.tv_sec - start_hr.tv_sec) +
                     (end_hr.tv_nsec - start_hr.tv_nsec) / 1e9;

    printf("Test completed after %d Toggle state changes.\n", toggle_change_count);
    printf("Total elapsed time: %.9f seconds.\n", elapsed);

    close(fd);
}

int main(int argc, char *argv[]) {
    int repetitions = 1;

    for (int i = 1; i < argc; i++) {
        if (isdigit(argv[i][0])) {
            int val = atoi(argv[i]);
            if (val > 0)
                max_toggles = val;
        } else if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--debug") == 0)) {
            debugMode = 1;
        } else if ((strcmp(argv[i], "-r") == 0) || (strcmp(argv[i], "--repetitions") == 0)) {
            if (i + 1 < argc && isdigit(argv[i + 1][0])) {
                repetitions = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Error: Missing argument for --repetitions\n");
                return 1;
            }
        }
    }

    for (int i = 0; i < repetitions; i++) {
        run_test(i + 1, repetitions);
        if (i < repetitions - 1) {
            printf("Pausing for 30 seconds before next run...\n");
            printf("\n");
            sleep(30);
        }
    }

    return 0;
}


