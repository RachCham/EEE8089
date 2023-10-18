// pid_controller.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define FREQ_FILE "frequency.txt"
#define CAPTURED_FREQ_FILE "captured_freq.txt"

double Kp, Ki, Kd;
double Km = 22.03584;  // Model's proportionality constant
double OFFSET = 737.0;

double Eold = 0.0;
double I = 0.0;
double setpoint = 0;

double computePID(double Ecurrent) {
    double P = Ecurrent; 
    I += Ecurrent;
    double D = Ecurrent - Eold;

    Eold = Ecurrent;

    double output = Kp * P + Ki * I + Kd * D;
    return output;
}

void control_motor(double freq) {
    double error = setpoint - freq;
    double output = computePID(error);

    double output_value = output * Km + OFFSET;
    output_value = round(output_value); // Rounding to ensure it's an integer

    // Send command to motor
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "echo %d %d \\\\ > /dev/serial0", (int)output_value, 1);
    system(cmd);

    // Capture the frequency to a file
    FILE *cap_file = fopen(CAPTURED_FREQ_FILE, "a");
    if (cap_file) {
        fprintf(cap_file, "%lf\n", freq);
        fclose(cap_file);
    } else {
        perror("Error writing to capture file");
        // Don't exit, because the primary function is motor control, not file I/O.
    }
}

void stop_motor() {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "echo 737 0 \\\\ > /dev/serial0");
    system(cmd);
}

void signal_handler(int sig) {
    stop_motor();
    exit(sig);
}

int main() {
    int inotify_fd, watch_fd;
    char buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
    FILE *file;

    // Handle the signals for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Clear the capture file at the start
    file = fopen(CAPTURED_FREQ_FILE, "w");
    if (file) {
        fclose(file);
    } else {
        perror("Error clearing capture file");
    }

    // Input PID constants from the user
    printf("Enter the value for Kp: ");
    scanf("%lf", &Kp);
    printf("Enter the value for Ki: ");
    scanf("%lf", &Ki);
    printf("Enter the value for Kd: ");
    scanf("%lf", &Kd);

    // Input the setpoint
    printf("Enter the desired frequency (setpoint) in Hz: ");
    scanf("%lf", &setpoint);

    inotify_fd = inotify_init();
    if (inotify_fd == -1) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

    watch_fd = inotify_add_watch(inotify_fd, FREQ_FILE, IN_MODIFY);
    if (watch_fd == -1) {
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }

    while (1) {
        ssize_t len = read(inotify_fd, buffer, sizeof(buffer));
        if (len == -1 && errno != EAGAIN) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (len <= 0) {
            break;
        }

        const struct inotify_event *event = (struct inotify_event *)buffer;
        if (event->mask & IN_MODIFY) {
            file = fopen(FREQ_FILE, "r");
            if (file) {
                double freq;
                fscanf(file, "%lf", &freq);
                fclose(file);

                control_motor(freq);
            }
        }
    }

    // Clean up
    inotify_rm_watch(inotify_fd, watch_fd);
    close(inotify_fd);

    return 0;
}
