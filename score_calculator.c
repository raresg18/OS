#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <hunt_id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *hunt_id = argv[1];

    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, "treasures.dat");

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        return EXIT_FAILURE;
    }

    Treasure t;
    int total_score = 0;

    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        total_score += t.value;
    }

    close(fd);

    printf("Total score for hunt '%s': %d\n", hunt_id, total_score);

    return EXIT_SUCCESS;
}
