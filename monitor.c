#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#define TREASURE_FILE "treasures.dat"
#define USERNAME_LEN 50
#define CLUE_LEN 255
#define SIGCALC (SIGRTMAX + 1)

typedef struct __attribute__((packed)) {
    int treasure_id;               
    char username[USERNAME_LEN];   
    float latitude;                 
    float longitude;                
    char clue[CLUE_LEN];           
    int value;                       
} Treasure;

int hub_pipe_fd[2];

void calculate_score(const char *hunt_id);

void list_hunts() {
    DIR *dir = opendir(".");
    if (!dir) {
        perror("ERROR: Failed to open current directory");
        fflush(stdout);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
            char file_path[256];
            snprintf(file_path, sizeof(file_path), "%s/%s", entry->d_name, TREASURE_FILE);

            struct stat st;
            int count = 0;
            if (stat(file_path, &st) == 0) {
                count = st.st_size / sizeof(Treasure);
            }
            printf("HUNT %s %d\n", entry->d_name, count);
            fflush(stdout);
        }
    }
    closedir(dir);
}

void list_treasures(const char *hunt_id) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("ERROR: Could not open treasure file for hunt");
        fflush(stdout);
        return;
    }

    printf("HUNT %s\n", hunt_id);
    fflush(stdout);

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("TREASURE %d %s %.6f %.6f %d\nCLUE: %s\n",
               t.treasure_id, t.username, t.latitude, t.longitude, t.value, t.clue);
        fflush(stdout);
    }
    close(fd);
}

void view_treasure(const char *hunt_id, int treasure_id) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("ERROR: Could not open treasure file for hunt");
        fflush(stdout);
        return;
    }

    Treasure t;
    int found = 0;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (t.treasure_id == treasure_id) {
            found = 1;
            printf("TREASURE %d %s %.6f %.6f %d\nCLUE: %s\n",
                   t.treasure_id, t.username, t.latitude, t.longitude, t.value, t.clue);
            fflush(stdout);
            break;
        }
    }
    if (!found) {
        printf("ERROR: Treasure ID %d not found in hunt %s\n", treasure_id, hunt_id);
        fflush(stdout);
    }
    close(fd);
}

void sigusr1_handler(int sig) {
    (void)sig;
    list_hunts();
}

void sigusr2_handler(int sig) {
    (void)sig;
    int fd = open("cmd_hunt.txt", O_RDONLY);
    if (fd == -1) {
        perror("ERROR: Could not open cmd_hunt.txt");
        fflush(stdout);
        return;
    }

    char hunt_id[256];
    ssize_t n = read(fd, hunt_id, sizeof(hunt_id) - 1);
    if (n > 0) {
        hunt_id[n] = '\0';
        hunt_id[strcspn(hunt_id, "\n")] = '\0';
        list_treasures(hunt_id);
    } else {
        printf("ERROR: cmd_hunt.txt is empty or malformed\n");
        fflush(stdout);
    }
    close(fd);
}

void sigrtmin_handler(int sig) {
    (void)sig;
    int fd = open("cmd_view.txt", O_RDONLY);
    if (fd == -1) {
        perror("ERROR: Could not open cmd_view.txt");
        fflush(stdout);
        return;
    }

    char hunt_id[256], treasure_id_str[256];
    ssize_t n = read(fd, hunt_id, sizeof(hunt_id) - 1);
    if (n > 0) {
        hunt_id[n] = '\0';
    }
    n = read(fd, treasure_id_str, sizeof(treasure_id_str) - 1);
    if (n > 0) {
        treasure_id_str[n] = '\0';
    }

    hunt_id[strcspn(hunt_id, "\n")] = '\0';
    treasure_id_str[strcspn(treasure_id_str, "\n")] = '\0';
    int treasure_id = atoi(treasure_id_str);
    
    view_treasure(hunt_id, treasure_id);
    close(fd);
}

void sigterm_handler(int sig) {
    (void)sig;
    printf("SIGTERM received: Exiting after 2 seconds...\n");
    fflush(stdout);
    usleep(2000000);
    exit(0);
}

void sigcalc_handler(int sig) {
    (void)sig;
    int fd = open("cmd_score.txt", O_RDONLY);
    if (fd == -1) {
        perror("ERROR: Could not open cmd_score.txt");
        fflush(stdout);
        return;
    }

    char hunt_id[256];
    if (read(fd, hunt_id, sizeof(hunt_id) - 1) > 0) {
        hunt_id[strcspn(hunt_id, "\n")] = '\0';
        calculate_score(hunt_id);
    } else {
        printf("ERROR: cmd_score.txt is empty or malformed\n");
        fflush(stdout);
    }
    close(fd);
}

void calculate_score(const char *hunt_id) {
    int score_pipe_fd[2];
    if (pipe(score_pipe_fd) == -1) {
        perror("Failed to create pipe for score calculation");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Failed to fork for score calculation");
        return;
    }

    if (pid == 0) {
        close(score_pipe_fd[0]);
        if (dup2(score_pipe_fd[1], STDOUT_FILENO) == -1) {
            perror("Failed to redirect stdout to pipe");
            exit(EXIT_FAILURE);
        }
        close(score_pipe_fd[1]);
        execl("./score_calculator", "score_calculator", hunt_id, NULL);
        perror("Failed to execute score_calculator");
        exit(EXIT_FAILURE);
    } else {
        close(score_pipe_fd[1]);
        char buf[1024];
        ssize_t n;
        printf("Score results for hunt '%s':\n", hunt_id);
        while ((n = read(score_pipe_fd[0], buf, sizeof(buf)-1)) > 0) {
            buf[n] = '\0';
            printf("%s", buf);
        }
        close(score_pipe_fd[0]);
        wait(NULL);
    }
}

int main() {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigusr2_handler;
    sigaction(SIGUSR2, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigrtmin_handler;
    sigaction(SIGRTMIN, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigcalc_handler;
    sigaction(SIGCALC, &sa, NULL);

    printf("Monitor running (PID: %d). Waiting for signals...\n", getpid());
    fflush(stdout);

    while (1) {
        pause();
    }

    return 0;
}
