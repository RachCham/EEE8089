#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

// Global Variables
int frequency_count = 0;
pthread_mutex_t freq_mutex;

// PID Controller Variables and Constants
double Kp = 1.0;
double Ki = 0.1;
double Kd = 0.01;
double Km = 22.03584;  // Model's proportionality constant

double Eold = 0.0;
double I = 0.0;

#define Tsample 0.02  // Closed-loop period of 20ms

#define OFFSET 737
#define MAX_OFFSET 500
#define MIN_SPEED (OFFSET - MAX_OFFSET)
#define MAX_SPEED (OFFSET + MAX_OFFSET)

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
    FILE *data_file = fopen("pid_response.txt", "w"); 
    if (data_file == NULL) {
        perror("Error opening data file");
        return NULL;
    }

    while (1) {
        usleep(20000);  // Sample every 20ms

        int current_interrupts;
        pthread_mutex_lock(&freq_mutex);
        current_interrupts = frequency_count;
        frequency_count = 0;
        pthread_mutex_unlock(&freq_mutex);

        // Convert to Frequency in Hz: (interruptions/500) / Tsample
        double actual_frequency = (current_interrupts / 500.0) / Tsample;

        // Reopen the file to clear its contents
        data_file = freopen("pid_response.txt", "w", data_file);
        if (data_file == NULL) {
            perror("Error reopening data file");
            return NULL;
        }

        fprintf(data_file, "%.2f\n", actual_frequency); // Write the data to the file
        fflush(data_file); // Flush the data to the file

        double error = setpoint - actual_frequency;
        double output = computePID(error);

        int output_value = OFFSET + output * Km;

        if (output_value < MIN_SPEED) output_value = MIN_SPEED;
        if (output_value > MAX_SPEED) output_value = MAX_SPEED;

        char cmd[128];
        snprintf(cmd, sizeof(cmd), "echo %d %d \\\\ > /dev/serial0", output_value, 1);
        system(cmd);  
    }

    fclose(data_file); // Close the data file
    return NULL;
}

void graceful_exit(int sig) {
    // Send command to stop motor
    system("echo 737 0 \\\\ > /dev/serial0");

    // Exit the program
    exit(0);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = graceful_exit;
    sa.sa_flags = 0; // or SA_RESTART;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    pthread_t thread1, thread2;
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

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_mutex_destroy(&freq_mutex);

    return 0;
}
