//
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <memory.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdlib.h>
#include <iostream>

void* ps_gpio_sysfs(void* unused) {
    // Export the desired pin by writing to /sys/class/gpio/export
	// base is 904, led is connected to mio47 -> 951
    auto fd = open("/sys/class/gpio/export", O_WRONLY);
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
    volatile unsigned int* gpio = (volatile unsigned int*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x41200000);
    close(fd);

    // Set direction as output for pin 2.
    *(gpio + 4) = 0xfffffffd;

    // Toggle LED 50 ms on, 50ms off
    while(1) {
    	*gpio = 0x00000000;
        usleep(50000);
    	*gpio = 0x00000002;
        usleep(50000);
    }

    return 0;
}

void* send_udp(void* unused) {
    int result = 0;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in addrListen = {}; // zero-int, sin_port is 0, which picks a random port for bind.
    addrListen.sin_family = AF_INET;
    result = bind(sock, (sockaddr*)&addrListen, sizeof(addrListen));
    if (result == -1)
    {
       int lasterror = errno;
       std::cout << "error: " << lasterror;
       exit(1);
    }


    sockaddr_storage addrDest = {};
    addrinfo* result_list = NULL;
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM; // without this flag, getaddrinfo will return 3x the number of addresses (one for each socket type).
    result = getaddrinfo("192.168.0.4", "9000", &hints, &result_list);
    if (result == 0)
    {
        //ASSERT(result_list->ai_addrlen <= sizeof(sockaddr_in));
        memcpy(&addrDest, result_list->ai_addr, result_list->ai_addrlen);
        freeaddrinfo(result_list);
    }


    if (result != 0)
    {
       int lasterror = errno;
       std::cout << "error: " << lasterror;
       exit(1);
    }

    const char* msg = "Jane Doe";
    size_t msg_length = strlen(msg);

    result = sendto(sock, msg, msg_length, 0, (sockaddr*)&addrDest, sizeof(addrDest));

    std::cout << result << " bytes sent" << std::endl;

    return 0;
}

int main() {
    pthread_t thread1, thread2, thread3;
    std::vector<uint8_t> eyal;
    eyal.push_back(2);
    printf("eyal[0] = %d\n", eyal[0]);
    pthread_create(&thread1, NULL, ps_gpio_sysfs, NULL);
    pthread_create(&thread2, NULL, pl_gpio_mmio, NULL);
    //pthread_create(&thread3, NULL, send_udp, NULL);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    //pthread_join(thread3, NULL);

	while(1);
    return 0;
}
