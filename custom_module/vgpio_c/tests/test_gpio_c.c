#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>

#define GPIO_MAGIC 'g'

struct gpio_data {
    int pin;
    int value;
};

#define GPIO_SET_VALUE _IOW(GPIO_MAGIC, 1, struct gpio_data)
#define GPIO_GET_VALUE _IOR(GPIO_MAGIC, 2, struct gpio_data)

#define DEVICE_FILE "/dev/vgpio_c"
volatile int gpio_change_count = 0;
int max_changes = 7;
int debugMode = 0;
int fd;
struct timespec start_hr, end_hr;
time_t start_time_print, end_time_print;

// Wait for Virtual Event in a separate thread
void *irq_wait_thread(void *arg)
{
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
void *sim_thread(void *arg)
{
    struct gpio_data data;
    srand(time(NULL));

    while (gpio_change_count < max_changes) {
        //usleep(1000);
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

int main(int argc, char *argv[])
{
    pthread_t thread_irq, thread_sim;

    // Parse command-line arguments.
    for (int i = 1; i < argc; i++) {
        if (isdigit(argv[i][0])) {
            int val = atoi(argv[i]);
            if (val > 0)
                max_changes = val;
        } else if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--debug") == 0)) {
            debugMode = 1;
        }
    }

    srand(time(NULL));

    fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Error opening device file");
        return 1;
    }

    // Store start time for printing
    time(&start_time_print);
    printf("Start time: %s", ctime(&start_time_print));

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

    // Stop high-resolution timer
    clock_gettime(CLOCK_MONOTONIC, &end_hr);
    double elapsed = (end_hr.tv_sec - start_hr.tv_sec) + 
                     (end_hr.tv_nsec - start_hr.tv_nsec) / 1e9;

    printf("Test completed after %d GPIO state changes.\n", gpio_change_count);
    printf("Total elapsed time: %.9f seconds.\n", elapsed);

    close(fd);
    return 0;
}
