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
pthread_cond_t start_printing_cond = PTHREAD_COND_INITIALIZER;
volatile int start_printing = 0;  // 控制何时开始打印频率的标志

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
    pthread_mutex_lock(&freq_mutex);
    while (!start_printing) {
        pthread_cond_wait(&start_printing_cond, &freq_mutex); // 等待主线程的信号开始打印
    }
    pthread_mutex_unlock(&freq_mutex);

    while (1) {
        usleep(20000);  // 20ms的采样率延迟

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

    printf("Press Enter to start...");
    getchar();  // 等待用户按下Enter键

    pthread_mutex_lock(&freq_mutex);
    start_printing = 1; // 设置标志开始打印频率
    pthread_cond_signal(&start_printing_cond); // 通知thread2_func线程开始打印
    pthread_mutex_unlock(&freq_mutex);

    while (1) {
        printf("Enter the desired output value (or type 'exit' to quit): ");
        char input[10];
        scanf("%s", input);
        if (strcmp(input, "exit") == 0) break;

        int output_value = atoi(input);
        sendOutputToMotor(output_value);  // 将输出值发送到电机
    }

    pthread_cancel(irq_thread);
    pthread_cancel(frequency_thread);
    pthread_mutex_destroy(&freq_mutex);
    return 0;
}
