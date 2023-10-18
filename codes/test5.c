#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#define FILE_PATH "/sys/class/gpio/gpio9/value"
#define SAMPLE_COUNT 20

// 全局变量
int frequency_count = 0;
pthread_mutex_t freq_mutex;

void *thread1_func(void *arg) {
    int gpio_fd;
    char buf[2];
    struct pollfd fds;

    gpio_fd = open(FILE_PATH, O_RDONLY);
    if (gpio_fd < 0) {
        perror("Failed to open file");
        exit(1);
    }

    fds.fd = gpio_fd;
    fds.events = POLLPRI;
    lseek(gpio_fd, 0, SEEK_SET);
    read(gpio_fd, buf, sizeof(buf));

    while (1) {
        lseek(gpio_fd, 0, SEEK_SET);
        read(gpio_fd, buf, sizeof(buf));
        poll(&fds, 1, -1);

        if (poll(&fds, 1, 1000)&&buf[0] == '1') {
            pthread_mutex_lock(&freq_mutex);
            frequency_count++;
            pthread_mutex_unlock(&freq_mutex);
        }
    }

    close(gpio_fd);
    return NULL;
}

void *thread2_func(void *arg) {
    double frequency_samples[SAMPLE_COUNT];
    int index = 0;

    while (1) {
        usleep(20000);  

        int current_frequency;
        pthread_mutex_lock(&freq_mutex);
        current_frequency = frequency_count;
        frequency_count = 0;
        pthread_mutex_unlock(&freq_mutex);

        double freq = (current_frequency * 50) / 500;
        frequency_samples[index] = freq;

        index++;
        if (index == SAMPLE_COUNT) {
            double sum = 0;
            for (int i = 0; i < SAMPLE_COUNT; i++) {
                sum += frequency_samples[i];
            }
            double average_frequency = sum / SAMPLE_COUNT;
            printf("Average Frequency of last %d samples: %.2f Hz\n", SAMPLE_COUNT, average_frequency);
            index = 0;
        }
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
