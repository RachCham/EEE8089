#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

// Global Variables
int frequency_count = 0;
pthread_mutex_t freq_mutex;

// PID Controller Variables and Constants
double Kp;
double Ki;
double Kd;
double Km = 22.03584;  // Model's proportionality constant

double Eold = 0.0;
double I = 0.0;

#define Tsample 0.02  // Closed-loop period of 20ms

#define OFFSET 737
#define MAX_OFFSET 500
#define MIN_SPEED (OFFSET - MAX_OFFSET)
#define MAX_SPEED (OFFSET + MAX_OFFSET)
#define DISPLAY_SAMPLE_COUNT 10
#define AVG_SAMPLE_COUNT 10

double setpoint = 0;

double computePID(double Ecurrent) {
    double P = Ecurrent; 
    I += Ecurrent * Tsample;
    double D = (Ecurrent - Eold) / Tsample;

    Eold = Ecurrent;

    double output = Kp * P + Ki * I + Kd * D;
    return output;
}

void *thread1_func(void *arg) {
    int fd = open("/sys/class/gpio/gpio9/value", O_RDONLY);
    if (fd == -1) {
        perror("Error opening gpio9 value");
        return NULL;
    }

    char buffer[2] = {0};
    char last_value = '0';

    while (1) {
        lseek(fd, 0, SEEK_SET);
        read(fd, buffer, sizeof(buffer) - 1);

        if (buffer[0] != last_value) {
            pthread_mutex_lock(&freq_mutex);
            frequency_count++;
            pthread_mutex_unlock(&freq_mutex);

            last_value = buffer[0];
        }
    }

    close(fd);
    return NULL;
}

void *thread2_func(void *arg) {
    double display_samples[DISPLAY_SAMPLE_COUNT];
    int index = 0;

    while (1) {
        usleep(20000);  // Sample every 20ms

        int current_interrupts;
        pthread_mutex_lock(&freq_mutex);
        current_interrupts = frequency_count;
        frequency_count = 0;
        display_samples[index] = (current_interrupts / 500.0) / Tsample;
        index = (index + 1) % DISPLAY_SAMPLE_COUNT;
        pthread_mutex_unlock(&freq_mutex);

        double actual_frequency = display_samples[(index + DISPLAY_SAMPLE_COUNT - 1) % DISPLAY_SAMPLE_COUNT];
        double error = setpoint - actual_frequency;
        double output = computePID(error);

        int output_value = OFFSET + output * Km;

        if (output_value < MIN_SPEED) output_value = MIN_SPEED;
        if (output_value > MAX_SPEED) output_value = MAX_SPEED;

        char cmd[128];
        snprintf(cmd, sizeof(cmd), "echo %d %d \\\\ > /dev/serial0", output_value, 1);
        system(cmd);  
    }

    return NULL;
}

void *thread3_func(void *arg) {
    double display_samples[DISPLAY_SAMPLE_COUNT];
    int index = 0;

    while (1) {
        usleep(20000 * DISPLAY_SAMPLE_COUNT); // Wait for the time required to gather 10 samples

        pthread_mutex_lock(&freq_mutex);
        double sum = 0;
        for (int i = 0; i < DISPLAY_SAMPLE_COUNT; i++) {
            sum += display_samples[i];
        }
        pthread_mutex_unlock(&freq_mutex);

        double average_frequency = sum / DISPLAY_SAMPLE_COUNT;
        printf("Average Frequency of last %d samples: %.2f Hz\n", DISPLAY_SAMPLE_COUNT, average_frequency);

        // Reset index for the next batch of samples
        index = 0;
    }

    return NULL;
}


int main() {
    pthread_t thread1, thread2, thread3;
    pthread_mutex_init(&freq_mutex, NULL);

    // Get PID parameters from the user
    printf("Enter the value for Kp: ");
    scanf("%lf", &Kp);
    printf("Enter the value for Ki: ");
    scanf("%lf", &Ki);
    printf("Enter the value for Kd: ");
    scanf("%lf", &Kd);

    printf("Enter the desired frequency (setpoint) in Hz: ");
    scanf("%lf", &setpoint);

    pthread_create(&thread1, NULL, thread1_func, NULL);
    pthread_create(&thread2, NULL, thread2_func, NULL);
    pthread_create(&thread3, NULL, thread3_func, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_mutex_destroy(&freq_mutex);

    return 0;
}
