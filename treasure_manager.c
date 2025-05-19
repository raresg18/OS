#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define USERNAME_LEN 50
#define CLUE_LEN 255
#define TEMP_FILE "temp.dat"
#define RECORD_FILE "treasures.dat"
#define LOG_FILE "logged_hunt"

typedef struct {
    int treasure_id;               
    char username[USERNAME_LEN]; 
    float latitude;         
    float longitude;        
    char clue[CLUE_LEN];             
    int value;                        
} Treasure;

void log_operation(const char *hunt_dir, const char *operation) {
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/%s", hunt_dir, LOG_FILE);
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd == -1) {
        perror("open log file");
        return;
    }

    time_t now = time(NULL);
    char log_entry[512];
    snprintf(log_entry, sizeof(log_entry), "%s: %s\n", ctime(&now), operation);

    if (write(log_fd, log_entry, strlen(log_entry)) != (ssize_t)strlen(log_entry)) {
        perror("write log entry");
    }
    close(log_fd);
}

int create_symlink_for_log(const char *hunt_id) {
    char target[256], linkpath[256];

    snprintf(target, sizeof(target), "./%s/%s", hunt_id, LOG_FILE);
    snprintf(linkpath, sizeof(linkpath), "logged_hunt-%s", hunt_id);

    unlink(linkpath);

    if (symlink(target, linkpath) == -1) {
        perror("symlink creation");
        return -1;
    }
    return 0;
}

int add_treasure(const char *hunt_id) {
    if (mkdir(hunt_id, 0755) == -1) {
        if (errno != EEXIST) {
            perror("mkdir");
            return -1;
        }
    }

    if (create_symlink_for_log(hunt_id) == -1) {
        fprintf(stderr, "Warning: Failed to create symlink for logged_hunt\n");
    }

    char record_path[256];
    snprintf(record_path, sizeof(record_path), "%s/%s", hunt_id, RECORD_FILE);

    int fd = open(record_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open treasures file");
        return -1;
    }

    Treasure treasure;
    
    printf("Enter treasure ID (integer): ");
    if (scanf("%d", &treasure.treasure_id) != 1) {
        fprintf(stderr, "Error reading treasure_id\n");
        close(fd);
        return -1;
    }
    getchar();

    printf("Enter username (max %d characters): ", USERNAME_LEN - 1);
    if (fgets(treasure.username, USERNAME_LEN, stdin) == NULL) {
        fprintf(stderr, "Error reading username\n");
        close(fd);
        return -1;
    }
    treasure.username[strcspn(treasure.username, "\n")] = '\0';

    printf("Enter latitude (floating point): ");
    if (scanf("%f", &treasure.latitude) != 1) {
        fprintf(stderr, "Error reading latitude\n");
        close(fd);
        return -1;
    }
    printf("Enter longitude (floating point): ");
    if (scanf("%f", &treasure.longitude) != 1) {
        fprintf(stderr, "Error reading longitude\n");
        close(fd);
        return -1;
    }
    getchar();

    printf("Enter clue text (max %d characters): ", CLUE_LEN - 1);
    if (fgets(treasure.clue, CLUE_LEN, stdin) == NULL) {
        fprintf(stderr, "Error reading clue text\n");
        close(fd);
        return -1;
    }
    treasure.clue[strcspn(treasure.clue, "\n")] = '\0';

    printf("Enter treasure value (integer): ");
    if (scanf("%d", &treasure.value) != 1) {
        fprintf(stderr, "Error reading value\n");
        close(fd);
        return -1;
    }

    ssize_t written = write(fd, &treasure, sizeof(Treasure));
    if (written != sizeof(Treasure)) {
        perror("write treasure record");
        close(fd);
        return -1;
    }
    close(fd);

    char log_details[256];
    snprintf(log_details, sizeof(log_details), "Added treasure ID %d by user %s", 
             treasure.treasure_id, treasure.username);
    log_operation(hunt_id, log_details);

    printf("Treasure added successfully.\n");
    return 0;
}

int list_treasures(const char *hunt_id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, RECORD_FILE);

    struct stat st;
    if (stat(file_path, &st) == -1) {
        perror("stat");
        return -1;
    }

    printf("Hunt: %s\n", hunt_id);
    printf("Total file size: %ld bytes\n", (long)st.st_size);

    int treasureFileOpen = open(file_path, O_RDONLY);
    if (treasureFileOpen == -1) {
        perror("open treasures file");
        return -1;
    }

    printf("\nTreasure List:\n");
    int count = 0;
    while (1) {
        Treasure treasure;
        ssize_t bytes_read = read(treasureFileOpen, &treasure, sizeof(Treasure));

        if (bytes_read == 0)
            break;
        if (bytes_read != sizeof(Treasure)) {
            fprintf(stderr, "Incomplete record read. File may be corrupted.\n");
            break;
        }

        count++;
        printf("Treasure #%d:\n", count);
        printf("  ID        : %d\n", treasure.treasure_id);
        printf("  Username  : %s\n", treasure.username);
        printf("  Latitude  : %.6f\n", treasure.latitude);
        printf("  Longitude : %.6f\n", treasure.longitude);
        printf("  Clue      : %s\n", treasure.clue);
        printf("  Value     : %d\n", treasure.value);
        printf("\n");
    }
    close(treasureFileOpen);

    if (count == 0) {
        printf("No treasures found in hunt '%s'.\n", hunt_id);
    }
    return 0;
}

int view_treasure(const char *hunt_id, int target_id) {
    char file_path[256];

    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, RECORD_FILE);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening treasures file");
        return -1;
    }

    Treasure treasure;
    int found = 0;

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (treasure.treasure_id == target_id) {
            printf("Treasure Details:\n");
            printf("  ID        : %d\n", treasure.treasure_id);
            printf("  Username  : %s\n", treasure.username);
            printf("  Latitude  : %.6f\n", treasure.latitude);
            printf("  Longitude : %.6f\n", treasure.longitude);
            printf("  Clue      : %s\n", treasure.clue);
            printf("  Value     : %d\n", treasure.value);
            found = 1;
            break;
        }
    }
    close(fd);

    if (!found) {
        fprintf(stderr, "Treasure with ID %d not found in hunt '%s'.\n", target_id, hunt_id);
        return -1;
    }

    return 0;
}

int remove_treasure(const char *hunt_id, int target_id) {
    char record_path[256];
    char temp_path[256];

    snprintf(record_path, sizeof(record_path), "%s/%s", hunt_id, RECORD_FILE);
    snprintf(temp_path, sizeof(temp_path), "%s/%s", hunt_id, TEMP_FILE);

    int fd_in = open(record_path, O_RDONLY);
    if (fd_in == -1) {
        perror("Error opening treasure file for reading");
        return -1;
    }

    int fd_out = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        perror("Error opening temporary file for writing");
        close(fd_in);
        return -1;
    }

    Treasure treasure;
    ssize_t bytes_read;
    int found = 0;


    while ((bytes_read = read(fd_in, &treasure, sizeof(Treasure))) == sizeof(Treasure)) {
        if (treasure.treasure_id == target_id) {
            found = 1;
            continue;
        }
        ssize_t bytes_written = write(fd_out, &treasure, sizeof(Treasure));
        if (bytes_written != sizeof(Treasure)) {
            perror("Error writing record to temporary file");
            close(fd_in);
            close(fd_out);
            unlink(temp_path);
            return -1;
        }
    }

    if (bytes_read == -1) {
        perror("Error reading treasure file");
        close(fd_in);
        close(fd_out);
        unlink(temp_path);
        return -1;
    }

    close(fd_in);
    close(fd_out);

    if (!found) {
        fprintf(stderr, "Treasure with ID %d not found in hunt '%s'.\n", target_id, hunt_id);
        unlink(temp_path);
        return -1;
    }

    if (unlink(record_path) == -1) {
        perror("Error removing original treasure file");
        unlink(temp_path);
        return -1;
    }
    if (rename(temp_path, record_path) == -1) {
        perror("Error renaming temporary file to original filename");
        return -1;
    }

    printf("Treasure with ID %d removed successfully.\n", target_id);
    return 0;
}

int remove_hunt(const char *hunt_id) {
    char hunt_dir[256];
    snprintf(hunt_dir, sizeof(hunt_dir), "./%s", hunt_id);

    char treasure_path[256];
    snprintf(treasure_path, sizeof(treasure_path), "%s/%s", hunt_dir, RECORD_FILE);

    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/%s", hunt_dir, LOG_FILE);

    if (unlink(treasure_path) == -1 && errno != ENOENT) {
        perror("Failed to delete treasures file");
        return -1;
    }

    if (unlink(log_path) == -1 && errno != ENOENT) {
        perror("Failed to delete logged_hunt file");
        return -1;
    }

    if (rmdir(hunt_dir) == -1) {
        perror("Failed to remove hunt directory (not empty?)");
        return -1;
    } else {
        printf("Successfully removed hunt directory: %s\n", hunt_dir);
    }

    char symlink_path[256];
    snprintf(symlink_path, sizeof(symlink_path), "logged_hunt-%s", hunt_id);
    if (unlink(symlink_path) == -1 && errno != ENOENT) {
        perror("Failed to remove symbolic link");
        return -1;
    } else {
        printf("Successfully removed symbolic link: %s\n", symlink_path);
    }

    return 0;
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s add <hunt_id>\n", argv[0]);
        fprintf(stderr, "  %s list <hunt_id>\n", argv[0]);
        fprintf(stderr, "  %s view <hunt_id> <treasure_id>\n", argv[0]);
        fprintf(stderr, "  %s remove_treasure <hunt_id> <treasure_id>\n", argv[0]);
        fprintf(stderr, "  %s remove_hunt <hunt_id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *command = argv[1];
    const char *hunt_id = argv[2];

    if (strcmp(command, "add") == 0) {
        return add_treasure(hunt_id);
    } else if (strcmp(command, "list") == 0) {
        return list_treasures(hunt_id);
    } else if (strcmp(command, "view") == 0 && argc == 4) {
        int id = atoi(argv[3]);
        return view_treasure(hunt_id, id);
    } else if (strcmp(command, "remove_treasure") == 0 && argc == 4) {
        int id = atoi(argv[3]);
        return remove_treasure(hunt_id, id);
    } else if (strcmp(command, "remove_hunt") == 0) {
        return remove_hunt(hunt_id);
    } else {
        fprintf(stderr, "Invalid command or arguments.\n");
        return EXIT_FAILURE;
    }
}