#include "../vgpio_c.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>

volatile int gpio_change_count = 0;
int max_changes = 7;
int debugMode = 0;
int fd;
struct timespec start_hr, end_hr;
time_t start_time_print, end_time_print;

// Wait for Virtual Event in a separate thread
void *irq_wait_thread(void *arg) {
    char buf;
    while (gpio_change_count < max_changes) {
        if (read(fd, &buf, 1) < 0) {
            perror("Error waiting for event");
            break;
        }
        gpio_change_count++;
        if (debugMode)
            printf("Virtual GPIO Event Triggered! Change #%d\n", gpio_change_count);
    }
    return NULL;
}

// Simulate GPIO changes
void *sim_thread(void *arg) {
    struct gpio_data data;
    srand(time(NULL));

    while (gpio_change_count < max_changes) {
        // usleep(1000);
        data.pin = rand() % 8;
        data.value = rand() % 2;
        if (ioctl(fd, GPIO_SET_VALUE, &data) < 0) {
            perror("Error setting GPIO value");
        } else if (debugMode) {
            printf("Simulated GPIO change: pin %d set to %d\n", data.pin, data.value);
        }
    }
    return NULL;
}

void run_test(int current_rep, int total_reps) {
    pthread_t thread_irq, thread_sim;

    // Reset the change count for each test run
    gpio_change_count = 0;

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Error opening device file");
        return;
    }

    // Print the repetition count
    printf("This is a test for the C kernel module\n");
    printf("Starting test %d out of %d\n", current_rep, total_reps);
    printf("\n");

    // Store start time for printing
    time(&start_time_print);
    printf("Start time: %s", ctime(&start_time_print));
    printf("Start Unix time: %ld\n", start_time_print);

    // Start high-resolution timer
    clock_gettime(CLOCK_MONOTONIC, &start_hr);

    if (debugMode)
        printf("Starting GPIO IRQ wait and simulation threads in DEBUG mode...\n");

    pthread_create(&thread_irq, NULL, irq_wait_thread, NULL);
    pthread_create(&thread_sim, NULL, sim_thread, NULL);

    pthread_join(thread_irq, NULL);
    pthread_join(thread_sim, NULL);

    // Store end time for printing
    time(&end_time_print);
    printf("End time: %s", ctime(&end_time_print));
    printf("End Unix time: %ld\n", end_time_print);

    // Stop high-resolution timer
    clock_gettime(CLOCK_MONOTONIC, &end_hr);
    double elapsed = (end_hr.tv_sec - start_hr.tv_sec) +
                     (end_hr.tv_nsec - start_hr.tv_nsec) / 1e9;

    printf("Test completed after %d GPIO state changes.\n", gpio_change_count);
    printf("Total elapsed time: %.9f seconds.\n", elapsed);

    close(fd);
}

int main(int argc, char *argv[]) {
    int repetitions = 1;

    // Parse command-line arguments.
    for (int i = 1; i < argc; i++) {
        if (isdigit(argv[i][0])) {
            int val = atoi(argv[i]);
            if (val > 0)
                max_changes = val;
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

