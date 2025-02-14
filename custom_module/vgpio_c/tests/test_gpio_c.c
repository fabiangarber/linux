#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // For strcmp()
#include <sys/ioctl.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>    // For isdigit()

#define GPIO_MAGIC 'g'

/* Must match the kernel’s definition */
struct gpio_data {
    int pin;
    int value;
};

#define GPIO_SET_VALUE _IOW(GPIO_MAGIC, 1, struct gpio_data)
#define GPIO_GET_VALUE _IOR(GPIO_MAGIC, 2, struct gpio_data)

#define NUM_GPIO_PINS 8
#define DEVICE_FILE "/dev/vgpio_c"

// Global file descriptor shared by both threads.
int fd;

// Global counter for the number of state changes.
volatile int gpio_change_count = 0;
// Global variable for maximum number of state changes (default is 7).
int max_changes = 7;

// Global flag to control debug output.
int debugMode = 0;

/* Poll thread: Waits indefinitely for a GPIO state change */
void *poll_thread(void *arg)
{
    struct pollfd pfd;
    int ret;
    
    pfd.fd = fd;
    pfd.events = POLLIN;
    
    while (gpio_change_count < max_changes) {
        ret = poll(&pfd, 1, -1);  // block indefinitely until an event
        if (ret < 0) {
            perror("Error polling device");
            break;
        }
        
        if (pfd.revents & POLLIN) {
            gpio_change_count++;
            if (debugMode) {
                printf("\nGPIO state changed (poll wakeup)! Change #%d\n", gpio_change_count);
                // Retrieve and print all GPIO states.
                for (int pin = 0; pin < NUM_GPIO_PINS; pin++) {
                    struct gpio_data data;
                    data.pin = pin;
                    ret = ioctl(fd, GPIO_GET_VALUE, &data);
                    if (ret < 0) {
                        perror("Error getting GPIO value");
                        break;
                    }
                    printf("GPIO[%d] = %d\n", data.pin, data.value);
                }
            }
        }
    }
    if (debugMode) {
        printf("Reached %d GPIO state changes. Exiting poll thread.\n", max_changes);
    }
    return NULL;
}

/* Simulation thread: Periodically toggles a GPIO value to ensure a state change */
void *sim_thread(void *arg)
{
    int ret;
    // Array to keep track of the last state for each pin.
    int current_state[NUM_GPIO_PINS] = {0};

    while (gpio_change_count < max_changes) {
        sleep(1);
        // Check again in case the poll thread already reached the limit.
        if (gpio_change_count >= max_changes)
            break;
        int pin = rand() % NUM_GPIO_PINS;
        // Toggle the state for the selected pin.
        current_state[pin] = !current_state[pin];

        struct gpio_data set_data;
        set_data.pin = pin;
        set_data.value = current_state[pin];
        ret = ioctl(fd, GPIO_SET_VALUE, &set_data);
        if (ret < 0) {
            perror("Error setting GPIO value");
        } else if (debugMode) {
            printf("Simulated GPIO change: pin %d set to %d\n", set_data.pin, set_data.value);
        }
    }
    if (debugMode) {
        printf("Reached %d GPIO state changes. Exiting simulation thread.\n", max_changes);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t thread_poll, thread_sim;
    time_t start_time, end_time;
    double elapsed;

    // Parse command-line arguments.
    // Usage: ./vgpio_test [max_changes] [--debug | -d]
    // Example: ./vgpio_test 123 -d
    for (int i = 1; i < argc; i++) {
        // If the argument is a number, use it for max_changes.
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
    
    // Record the start timestamp.
    time(&start_time);
    printf("Start time: %s", ctime(&start_time));

    if (debugMode) {
        printf("Starting GPIO poll and simulation threads in DEBUG mode...\n");
    }
    
    if (pthread_create(&thread_poll, NULL, poll_thread, NULL)) {
        perror("Error creating poll thread");
        close(fd);
        return 1;
    }
    
    if (pthread_create(&thread_sim, NULL, sim_thread, NULL)) {
        perror("Error creating simulation thread");
        close(fd);
        return 1;
    }
    
    // Wait for both threads to finish.
    pthread_join(thread_poll, NULL);
    pthread_join(thread_sim, NULL);
    
    // Record the end timestamp.
    time(&end_time);
    printf("End time: %s", ctime(&end_time));

    // Compute and print the elapsed time in seconds.
    elapsed = difftime(end_time, start_time);
    printf("Test completed after %d GPIO state changes.\n", gpio_change_count);
    printf("Total elapsed time: %.2f seconds.\n", elapsed);
    
    close(fd);
    return 0;
}
