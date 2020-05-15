//
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

void* ps_gpio_sysfs(void* unused) {
    // Export the desired pin by writing to /sys/class/gpio/export
	// base is 904, led is connected to mio47 -> 951
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1) {
        perror("Unable to open /sys/class/gpio/export");
        exit(1);
    }
    if (!write(fd, "951", 3)) {
        perror("Error writing to /sys/class/gpio/export");
        printf("Pin was already exported.");
    }
    close(fd);

    // Set the pin to be an output by writing "out" to direction
    fd = open("/sys/class/gpio/gpio951/direction", O_WRONLY);
    if (fd == -1) {
        perror("Unable to open /sys/class/gpio/gpio951/direction");
        exit(1);
    }
    if (!write(fd, "out", 3)) {
        perror("Error writing to /sys/class/gpio/gpio951/direction");
        exit(1);
    }
    close(fd);

    // Toggle LED 50 ms on, 50ms off
    fd = open("/sys/class/gpio/gpio951/value", O_WRONLY);
    if (fd == -1) {
        perror("Unable to open /sys/class/gpio/gpio951/value");
        exit(1);
    }
    while(1) {
        if (!write(fd, "1", 1)) {
            perror("Error writing to /sys/class/gpio/gpio951/value");
            exit(1);
        }
        usleep(50000);

        if (!write(fd, "0", 1)) {
            perror("Error writing to /sys/class/gpio/gpio951/value");
            exit(1);
        }
        usleep(50000);
    }
    close(fd);

    // Unexport the pin by writing to /sys/class/gpio/unexport
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1) {
        perror("Unable to open /sys/class/gpio/unexport");
        exit(1);
    }
    if (!write(fd, "951", 3)) {
        perror("Error writing to /sys/class/gpio/unexport");
        exit(1);
    }
    close(fd);

    return 0;
}

// For some reason this requires a first time devmem access
// on the machine:
// devmem 0x41200004 32 0xfffffffd
// devmem 0x41200000 32 0x00000000
// devmem 0x41200000 32 0x00000002
// and then it will work.
void* pl_gpio_mmio(void* unused) {
    // Open.
    int fd = open("/dev/mem", O_RDWR);
    if (fd == -1) {
        perror("Unable to open /dev/mem");
        exit(1);
    }
    volatile unsigned long* gpio = (volatile unsigned long*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x41200000);
    close(fd);

    // Set direction as output for pin 2.
    *(gpio + 4) = 0xfffffffd;

    // Toggle LED 50 ms on, 50ms off.
    while(1) {
    	*gpio = 0x00000000;
        usleep(50000);
    	*gpio = 0x00000002;
        usleep(50000);
    }

    return 0;
}

int main() {
    pthread_t thread1, thread2;
    int eyal = 5;
    printf("eyal = %d\n", eyal);
    pthread_create(&thread1, NULL, ps_gpio_sysfs, NULL);
    pthread_create(&thread2, NULL, pl_gpio_mmio, NULL);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

	while(1);
    return 0;
}
