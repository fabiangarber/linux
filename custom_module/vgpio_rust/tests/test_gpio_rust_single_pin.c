#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

// These constants must match those defined in your Rust module.
#define GPIO_SET_VALUE 0x40086701
#define GPIO_GET_VALUE 0x80086702

// This structure must match the C-compatible structure in your Rust module.
struct gpio_data {
    int pin;
    int value;
};

int main(void) {
    int fd;
    struct gpio_data data;
    int ret;

    // Open the character device.
    fd = open("/dev/vgpio_rust", O_RDWR);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Set GPIO pin 0 to 1.
    data.pin = 0;
    data.value = 1;
    ret = ioctl(fd, GPIO_SET_VALUE, &data);
    if (ret < 0) {
        fprintf(stderr, "ioctl(GPIO_SET_VALUE) failed: %s\n", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    printf("Set GPIO pin %d to %d\n", data.pin, data.value);

    // Now, get the current value of GPIO pin 0.
    // Initialize data.value to 0 (optional).
    data.value = 0;
    ret = ioctl(fd, GPIO_GET_VALUE, &data);
    if (ret < 0) {
        fprintf(stderr, "ioctl(GPIO_GET_VALUE) failed: %s\n", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    printf("Read GPIO pin %d, value = %d\n", data.pin, data.value);

    close(fd);
    return 0;
}
