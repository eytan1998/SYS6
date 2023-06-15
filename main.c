#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>

#define DEVICE_PATH "/dev/chardev"
#define ENCRYPT _IOW('k', 1, int)  // IOCTL command to update the encryption key


#define TRUE 1
#define ERROR 1
#define FALSE 0


int main(int argc, char *argv[]) {
    int isWrite = FALSE;
    int isRead = FALSE;
    int isKey = FALSE;
    int newKey = 0;
    char *message = "";

    if (argc < 2) {
        printf("usage: ./main -w \"message to write\" -k <new key> -r\n");
        return ERROR;
    }

    for (int i = 1; i < 6; ++i) {
        if (argv[i] == NULL) break;
        if (strcmp(argv[i], "-w") == 0) {
            isWrite = TRUE;
            message = argv[++i];
        } else if (strcmp(argv[i], "-k") == 0) {
            isKey = TRUE;
            newKey = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0) {
            isRead = TRUE;
        } else {
            printf("usage: ./main -w \"message to write\" -k <new key> -r\n");
            return ERROR;
        }
    }
    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return ERROR;
    }


/* ###################################
*           key
 * ###################################
*/
    if (isKey) {
        // Update the encryption key using IOCTL
        if (ioctl(fd, ENCRYPT, &newKey) < 0) {
            perror("IOCTL failed");
            close(fd);
            return ERROR;
        }

        printf("Encryption key updated to %d\n", newKey);
    }

/* ###################################
*           write
 * ###################################
*/
    if (isWrite) {
        // Write data to the device
        ssize_t bytes_written = write(fd, message, strlen(message));
        if (bytes_written < 0) {
            perror("Write failed");
            close(fd);
            return ERROR;
        }
        printf("Data written to the device: %s\n", message);
    }
/* ###################################
*           read
 * ###################################
*/
    if (isRead) {
        // Read data from the device
        char read_buffer[1024];
        ssize_t bytes_read = read(fd, read_buffer, sizeof(read_buffer));
        if (bytes_read < 0) {
            perror("Read failed");
            close(fd);
            return ERROR;
        }
        if (bytes_read == 0) {
            printf("Empty.\n");
        } else {
            read_buffer[bytes_read] = '\0';
            printf("Data read from the device: %s\n", read_buffer);
        }
    }


    close(fd);
    return 0;
}
