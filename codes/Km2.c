#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>

#define GPIO_PATH "/sys/class/gpio/gpio9/value"

volatile int frequency_count = 0;
pthread_mutex_t freq_mutex;

void sendOutputToMotor(int value) {
    char command[100];
    snprintf(command, sizeof(command), "echo %d > /dev/serial0", value);
    system(command);
}

void *readIRQThread(void *arg) {
    int fd = open(GPIO_PATH, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open GPIO for reading");
        return NULL;
    }

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLPRI;

    char dummy;
    read(fd, &dummy, 1); // Initial read

    while (1) {
        lseek(fd, 0, SEEK_SET);
        poll(&pfd, 1, -1); // Block until an event is detected

        char value;
        read(fd, &value, 1);
        if (value == '1') {
            pthread_mutex_lock(&freq_mutex);
            frequency_count++;
            pthread_mutex_unlock(&freq_mutex);
        }
    }
    close(fd);
}

void *thread2_func(void *arg) {
    while (1) {
        usleep(20000);  // Sampling rate delay of 20ms

        int current_frequency;

        pthread_mutex_lock(&freq_mutex);
        current_frequency = frequency_count;
        frequency_count = 0;
        pthread_mutex_unlock(&freq_mutex);

        printf("Frequency: %.2f Hz\n", (current_frequency / 500.0) / 0.02); 
    }

    return NULL;
}

int main() {
    printf("Enter the initial desired output value: ");
    char input[10];
    scanf("%s", input);
    int output_value = atoi(input);
    sendOutputToMotor(output_value);  // Send the initial output value to the motor

    pthread_t irq_thread, frequency_thread;
    pthread_mutex_init(&freq_mutex, NULL);

    if (pthread_create(&irq_thread, NULL, readIRQThread, NULL) != 0) {
        perror("Failed to create IRQ thread");
        return 1;
    }

    if (pthread_create(&frequency_thread, NULL, thread2_func, NULL) != 0) {
        perror("Failed to create frequency computation thread");
        return 1;
    }

    while (1) {
        printf("Enter the desired output value (or type 'exit' to quit): ");
        scanf("%s", input);
        if (strcmp(input, "exit") == 0) break;

        output_value = atoi(input);
        sendOutputToMotor(output_value);  // Send the output value to the motor
    }

    pthread_cancel(irq_thread);
    pthread_cancel(frequency_thread);
    pthread_mutex_destroy(&freq_mutex);
    return 0;
}
