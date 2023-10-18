// frequency_output.c

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#define FILE_PATH "/sys/class/gpio/gpio9/value"

// Global Variables
int frequency_count = 0;
pthread_mutex_t freq_mutex;

void *thread1_func(void *arg) {
    int gpio_fd;
    char buf[2];
    struct pollfd fds;

    gpio_fd = open(FILE_PATH, O_RDONLY | O_NONBLOCK);
    if (gpio_fd < 0) {
        perror("Failed to open file");
        exit(1);
    }

    fds.fd = gpio_fd;
    fds.events = POLLPRI;

  //  lseek(gpio_fd, 0, SEEK_SET);
//    read(gpio_fd, buf, sizeof(buf));

    while (1) {
        poll(&fds, 1, -1);
        lseek(gpio_fd, 0, SEEK_SET);
        read(gpio_fd, buf, sizeof(buf));


        if (buf[0] == '1') {
            pthread_mutex_lock(&freq_mutex);
            frequency_count++;
            pthread_mutex_unlock(&freq_mutex);
        }
    }

    close(gpio_fd);
    return NULL;
}

void *thread2_func(void *arg) {
    while (1) {
        usleep(20000);  // Sleep for 20ms

        pthread_mutex_lock(&freq_mutex);
        int current_frequency = frequency_count;
        frequency_count = 0;
        pthread_mutex_unlock(&freq_mutex);

        double actual_frequency = (current_frequency / 500.0) / 0.02;

        // Write to file
        FILE *file = fopen("frequency.txt", "w");
        if (!file) {
            perror("Failed to open frequency.txt");
            exit(1);
        }
        fprintf(file, "%.2f", actual_frequency);
        fclose(file);
    }

    return NULL;
}

int main() {
    pthread_t thread1, thread2;
    pthread_mutex_init(&freq_mutex, NULL);

    pthread_create(&thread1, NULL, thread1_func, NULL);
    pthread_create(&thread2, NULL, thread2_func, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_mutex_destroy(&freq_mutex);
    return 0;
}
