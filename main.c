#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/seqchardev"
#define ENCRYPT _IOW('k', 1, int)  // IOCTL command to update the encryption key

int main() {
    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return 1;
    }

    // Write data to the device
    char write_buffer[] = "AAAA";
    ssize_t bytes_written = write(fd, write_buffer, sizeof(write_buffer) - 1);
    if (bytes_written < 0) {
        perror("Write failed");
        close(fd);
        return 1;
    }

    printf("Data written to the device: %s\n", write_buffer);

    // Read data from the device
    char read_buffer[1024];
    ssize_t bytes_read = read(fd, read_buffer, sizeof(read_buffer));
    if (bytes_read < 0) {
        perror("Read failed");
        close(fd);
        return 1;
    }

    read_buffer[bytes_read] = '\0';
    printf("Data read from the device: %s\n", read_buffer);

    // Update the encryption key using IOCTL
    int new_key = 0;
    printf("input key:");
    scanf("%d", &new_key);
    if (new_key != 0) {
        if (ioctl(fd, ENCRYPT, &new_key) < 0) {
            perror("IOCTL failed");
            close(fd);
            return 1;
        }
    }
    printf("Encryption key updated to %d\n", new_key);

    close(fd);
    return 0;
}
