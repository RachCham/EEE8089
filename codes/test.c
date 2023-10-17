#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#define FILE_PATH "/sys/class/gpio/gpio9/value"  // 使用模拟的文件路径

// 全局变量
float frequency_count = 0;            // 频率计数器
pthread_mutex_t freq_mutex;         // 用于保护频率计数器的互斥锁

void *thread1_func(void *arg) {
    int gpio_fd;
    char buf[2];  // 增加了一个字符，用于读取换行符
    struct pollfd fds;

    // 使用open函数打开对应的文件
    gpio_fd = open(FILE_PATH, O_RDONLY);
    if (gpio_fd < 0) {
        perror("Failed to open file");
        exit(1);
    }

    fds.fd = gpio_fd;
    fds.events = POLLPRI;

    while (1) {
        lseek(gpio_fd, 0, SEEK_SET); // 每次循环都要重置文件指针到开始位置
        read(gpio_fd, buf, sizeof(buf)); // 读取文件的当前值
        poll(&fds, 1, 1000);  // 修改等待时间以适应模拟

        if (buf[0] == '1') {  // 检查是否为高电平
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
        usleep(20000);  // 采样率延迟20ms

        float current_frequency;

        pthread_mutex_lock(&freq_mutex);
        current_frequency = frequency_count;
        frequency_count = 0;
        pthread_mutex_unlock(&freq_mutex);
        
        printf("Frequency: %f Hz\n", current_frequency * 50 / 500); 
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
