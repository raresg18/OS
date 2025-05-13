#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

pid_t monitor_pid = -1;
extern pid_t monitor_pid;

void handle_sigchld(int sig) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid > 0) {
        printf("[Hub] Monitor (PID %d) has terminated.\n", pid);
        monitor_pid = -1;
    }
}

void start_monitor() {
    if (monitor_pid != -1) {
        printf("[Hub] Monitor is already running (PID: %d)\n", monitor_pid);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("[Hub] fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        execl("./monitor", "monitor", NULL);
        perror("[Hub] Failed to start monitor process");
        exit(EXIT_FAILURE);
    } else {
        monitor_pid = pid;
        printf("[Hub] Monitor started with PID: %d\n", monitor_pid);
    }
}

void list_hunts() {
    if (monitor_pid == -1) {
        printf("[Hub] No monitor running. Start it first.\n");
        return;
    }
    printf("[Hub] Sending request to list hunts...\n");
    if (kill(monitor_pid, SIGUSR1) == -1) {
        perror("[Hub] Failed to send SIGUSR1 to monitor");
    }
}

void handle_list_treasures(char *input) {
    char *token = strtok(input, " ");
    token = strtok(NULL, " ");
    if (!token) {
        printf("[Hub] Usage: list_treasures <hunt_id>\n");
        return;
    }
    int fd = open("cmd_hunt.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("[Hub] Failed to open cmd_hunt.txt");
        return;
    }
    size_t len = strlen(token);
    char buf[len + 2];
    strcpy(buf, token);
    strcat(buf, "\n");
    if (write(fd, buf, strlen(buf)) == -1) {
        perror("[Hub] Failed to write hunt id to cmd_hunt.txt");
        close(fd);
        return;
    }
    close(fd);
    printf("[Hub] Requesting list_treasures for hunt '%s'...\n", token);
    if (kill(monitor_pid, SIGUSR2) == -1) {
        perror("[Hub] Failed to send SIGUSR2 to monitor");
    }
}

void handle_view_treasure(char *input) {
    char *token = strtok(input, " ");
    char *hunt_id = strtok(NULL, " ");
    char *treasure_id = strtok(NULL, " ");
    if (!hunt_id || !treasure_id) {
        printf("[Hub] Usage: view_treasure <hunt_id> <treasure_id>\n");
        return;
    }
    
    int fd = open("cmd_view.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("[Hub] Failed to open cmd_view.txt");
        return;
    }
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s\n%s\n", hunt_id, treasure_id);
    if (write(fd, buffer, strlen(buffer)) == -1) {
        perror("[Hub] Failed to write to cmd_view.txt");
        close(fd);
        return;
    }
    close(fd);
    printf("[Hub] Requesting view_treasure for hunt '%s' and treasure '%s'...\n", hunt_id, treasure_id);
    if (kill(monitor_pid, SIGRTMIN) == -1) {
        perror("[Hub] Failed to send SIGRTMIN to monitor");
    }
}

void stop_monitor() {
    if (monitor_pid == -1) {
        printf("[Hub] No monitor is running.\n");
        return;
    }
    
    printf("[Hub] Sending termination request to monitor (PID: %d)...\n", monitor_pid);
    if (kill(monitor_pid, SIGTERM) == -1) {
        perror("[Hub] Failed to send SIGTERM to monitor");
        return;
    }
    
    while (monitor_pid != -1) {
        sleep(1);
    }
    printf("[Hub] Monitor terminated.\n");
}

void exit_hub() {
    if (monitor_pid != -1) {
        printf("[Hub] Cannot exit: Monitor is still running (PID: %d)\n", monitor_pid);
        return;
    }
    printf("[Hub] Exiting Treasure Hub.\n");
    exit(EXIT_SUCCESS);
}

int main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sa, NULL);

    char command[256];
    printf("Welcome to Treasure Hub\n");
    while (1) {
        printf("hub> ");
        if (fgets(command, sizeof(command), stdin) == NULL)
            break;
        command[strcspn(command, "\n")] = '\0';

       if (strcmp(command, "start_monitor") == 0) {
            start_monitor();
        } else if (strcmp(command, "list_hunts") == 0) {
            list_hunts();
        } else if (strncmp(command, "list_treasures", 14) == 0) {
            handle_list_treasures(command);
        } else if (strncmp(command, "view_treasure", 13) == 0) {
            handle_view_treasure(command);
        } else if (strcmp(command, "stop_monitor") == 0) {
            stop_monitor();
        } else if (strcmp(command, "exit") == 0) {
            exit_hub();
        } else {
            printf("[Hub] Unknown or unimplemented command: %s\n", command);
        }
    }

    return 0;
}
