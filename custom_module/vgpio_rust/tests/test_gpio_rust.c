#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

// IOCTL command constants (must match the kernel module)
#define GPIO_SET_VALUE 0x40086701
#define GPIO_GET_VALUE 0x80086702

// Structure for GPIO data passed through ioctl
struct GpioData {
    int pin;
    int value;
};

void toggle_pins(int fd, int num_toggles) {
    struct GpioData data;
    for (int i = 0; i < num_toggles; i++) {
        data.pin = rand() % 8; // Random pin between 0 and 7
        data.value = rand() % 2; // Random value 0 or 1
        if (ioctl(fd, GPIO_SET_VALUE, &data) < 0) {
            perror("ioctl set value");
            exit(EXIT_FAILURE);
        }
        //printf("Simulated GPIO change: pin %d set to %d\n", data.pin, data.value);
    }
}

void run_test(int current_rep, int total_reps, int fd, int num_toggles) {
    printf("Starting test %d out of %d\n", current_rep, total_reps);
    printf("\n");

    // Store start time for printing
    time_t start_time_print;
    time(&start_time_print);
    printf("Start time: %s", ctime(&start_time_print));

    // Start high-resolution timer
    struct timespec start_hr, end_hr;
    clock_gettime(CLOCK_MONOTONIC, &start_hr);

    toggle_pins(fd, num_toggles);

    // Store end time for printing
    time_t end_time_print;
    time(&end_time_print);
    printf("End time: %s", ctime(&end_time_print));

    // Stop high-resolution timer
    clock_gettime(CLOCK_MONOTONIC, &end_hr);
    double elapsed = (end_hr.tv_sec - start_hr.tv_sec) +
                     (end_hr.tv_nsec - start_hr.tv_nsec) / 1e9;

    printf("Test completed after %d GPIO state changes.\n", num_toggles);
    printf("Total elapsed time: %.9f seconds.\n", elapsed);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_toggles> <num_runs>\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("This is a test for the Rust kernel module.\n");

    int num_toggles = atoi(argv[1]);
    int num_runs = atoi(argv[2]);

    int fd = open("/dev/vgpio_rust", O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    srand(time(NULL)); // Seed the random number generator

    for (int i = 0; i < num_runs; i++) {
        run_test(i + 1, num_runs, fd, num_toggles);
        if (i < num_runs - 1) {
            printf("Pausing for 23 seconds before next run...\n");
            printf("\n");
            sleep(23);
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}
