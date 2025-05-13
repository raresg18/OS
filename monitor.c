#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#define TREASURE_FILE "treasures.dat"
#define USERNAME_LEN 50
#define CLUE_LEN 255

typedef struct __attribute__((packed)) {
    int treasure_id;
    char username[USERNAME_LEN];
    float latitude;
    float longitude;
    char clue[CLUE_LEN];
    int value;
} Treasure;

void list_hunts() {
    DIR *dir = opendir(".");
    if (!dir) {
        perror("[Monitor] Failed to open current directory");
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
            printf("[Monitor] Hunt: %s | Treasures: %d\n", entry->d_name, count);
        }
    }
    closedir(dir);
}

void list_treasures(const char *hunt_id) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", hunt_id, TREASURE_FILE);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("[Monitor] Could not open treasure file");
        return;
    }

    printf("[Monitor] Treasures in Hunt '%s':\n", hunt_id);
    Treasure t;
    while (fread(&t, sizeof(Treasure), 1, fp) == 1) {
        printf("  ID: %d | User: %s | Location: %.6f, %.6f | Value: %d\n", 
               t.treasure_id, t.username, t.latitude, t.longitude, t.value);
        printf("     Clue: %s\n", t.clue);
    }

    fclose(fp);
}

void view_treasure(const char *hunt_id, int treasure_id) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", hunt_id, TREASURE_FILE);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("[Monitor] Could not open treasure file");
        return;
    }

    Treasure t;
    int found = 0;
    while (fread(&t, sizeof(Treasure), 1, fp) == 1) {
        if (t.treasure_id == treasure_id) {
            found = 1;
            printf("[Monitor] Treasure ID %d in hunt '%s':\n", treasure_id, hunt_id);
            printf("  User: %s | Location: %.6f, %.6f | Value: %d\n", 
                   t.username, t.latitude, t.longitude, t.value);
            printf("  Clue: %s\n", t.clue);
            break;
        }
    }

    if (!found) {
        printf("[Monitor] Treasure ID %d not found in hunt '%s'.\n", treasure_id, hunt_id);
    }

    fclose(fp);
}

void sigusr1_handler(int sig) {
    (void)sig;
    printf("[Monitor] SIGUSR1 → list_hunts\n");
    list_hunts();
}

void sigusr2_handler(int sig) {
    (void)sig;
    printf("[Monitor] SIGUSR2 → list_treasures\n");

    FILE *fp = fopen("cmd_hunt.txt", "r");
    if (!fp) {
        perror("[Monitor] Could not open cmd_hunt.txt");
        return;
    }

    char hunt_id[256];
    if (fgets(hunt_id, sizeof(hunt_id), fp)) {
        hunt_id[strcspn(hunt_id, "\n")] = '\0';
        list_treasures(hunt_id);
    } else {
        fprintf(stderr, "[Monitor] cmd_hunt.txt is empty or malformed.\n");
    }

    fclose(fp);
}

void sigrtmin_handler(int sig) {
    (void)sig;
    printf("[Monitor] SIGRTMIN → view_treasure\n");

    FILE *fp = fopen("cmd_view.txt", "r");
    if (!fp) {
        perror("[Monitor] Could not open cmd_view.txt");
        return;
    }

    char hunt_id[256], treasure_id_str[256];
    if (!fgets(hunt_id, sizeof(hunt_id), fp) || !fgets(treasure_id_str, sizeof(treasure_id_str), fp)) {
        fprintf(stderr, "[Monitor] cmd_view.txt malformed.\n");
        fclose(fp);
        return;
    }

    hunt_id[strcspn(hunt_id, "\n")] = '\0';
    treasure_id_str[strcspn(treasure_id_str, "\n")] = '\0';
    int treasure_id = atoi(treasure_id_str);

    view_treasure(hunt_id, treasure_id);
    fclose(fp);
}

void sigterm_handler(int sig) {
    (void)sig;
    printf("[Monitor] SIGTERM → exiting after 2 seconds...\n");
    usleep(2000000);
    exit(0);
}

int main() {
    printf("[Monitor] Running (PID: %d). Waiting for signals...\n", getpid());

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

    while (1) {
        pause();
    }

    return 0;
}
