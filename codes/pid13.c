#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <poll.h>  // Added for the poll function

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
#define MAX_OFFSET 313
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

void graceful_exit(int sig) {
    // Send command to stop motor
    system("echo 737 0 \\\\ > /dev/serial0");

    // Exit the program
    exit(0);
}

void *thread1_func(void *arg) {
    int file_descriptor = open("/sys/class/gpio/gpio9/value", O_RDONLY | O_NONBLOCK);
    if (file_descriptor == -1) {
        perror("Error opening gpio9 value");
        return NULL;
    }

    char buffer[2] = {0};

    struct pollfd fds;
    fds.fd = file_descriptor;
    fds.events = POLLPRI | POLLERR;

    while (1) {
        lseek(file_descriptor, 0, SEEK_SET);
        read(file_descriptor, buffer, sizeof(buffer) - 1);

        int poll_ret = poll(&fds, 1, 1000);

        if (poll_ret > 0) {
            if (fds.revents & POLLPRI) {
                lseek(file_descriptor, 0, SEEK_SET);
                read(file_descriptor, buffer, sizeof(buffer) - 1);

                if (buffer[0] == '1') {
                    pthread_mutex_lock(&freq_mutex);
                    frequency_count++;
                    pthread_mutex_unlock(&freq_mutex);
                }

                
            }
        } else if (poll_ret < 0) {
            perror("Poll error");
        }
    }

    close(file_descriptor);
    return NULL;
}


void *thread2_func(void *arg) {
    FILE *data_file = fopen("pid_response.txt", "w"); // Open a file to overwrite the PID response data
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

int main() {
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

    // Set signal handlers for multiple signals to ensure we gracefully exit
    signal(SIGINT, graceful_exit);    // Interrupt from keyboard (Ctrl+C)
    signal(SIGTERM, graceful_exit);   // Termination signal
    signal(SIGQUIT, graceful_exit);   // Quit from keyboard

    pthread_create(&thread1, NULL, thread1_func, NULL);
    pthread_create(&thread2, NULL, thread2_func, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_mutex_destroy(&freq_mutex);

    return 0;
}
