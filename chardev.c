#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/time.h>
#include <linux/device.h>

#define DEVICE_NAME "seqchardev"  // Device name to be created
#define CLASS_NAME "seqchardev_class"  // Class name for the device

#define BUF_LEN 1024              // Maximum length of the message buffer

#define ENCRYPT _IOW('k', 1, int)  // IOCTL command to update the encryption key
#define GET_INFO _IOR('k', 2, int) // IOCTL command to get device information

static int major_number;          // Major number assigned to our device driver
static struct class *chardev_class; // Device class
static struct device *chardev_device; // Device structure
static char message[BUF_LEN];     // Message buffer to store data
static int message_size = 0;      // Current size of the message buffer
static int encryption_key = 0;    // Encryption key

module_param(encryption_key, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(encryption_key, "An integer");

struct device_info {
    int major_number;
    int encryption_key;
};

// Function to encrypt/decrypt data using the encryption key
static void encrypt_data(char *data, size_t length) {
    char *keyBytes = (char *) &encryption_key; // Treat the int as an array of bytes

    for (int j = 0; j < length;) {
        int iter = 4;
        for (int i = 0; i < iter; ++i) {
            data[j + i] = data[j + i] ^ keyBytes[i];
        }
        j += 4;
    }
}

// Function to open the device
static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO
    "Device opened\n");
    return 0;
}

// Function to release the device
static int device_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO
    "Device closed\n");
    return 0;
}

// Function to read data from the device
static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset){
int bytes_read = 0;

if (*offset >= message_size)
    return 0;  // End of file

// Copy data from kernel space to user space
    bytes_read = min(length, (size_t)(message_size - *offset));
    if (copy_to_user(buffer, message + *offset, bytes_read))
        return -EFAULT;

    *offset += bytes_read;
    return bytes_read;
}

// Function to write data to the device
static ssize_t device_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset) {
    int bytes_written = 0;

    // Check if the message buffer has enough space
    if (length > BUF_LEN - message_size)
        return - ENOSPC;  // No space left on device

    // Copy data from user space to kernel space
    if (copy_from_user(message + message_size, buffer, length))
        return -EFAULT;

    encrypt_data(message + message_size, length);
    bytes_written = length;

    message_size +=  bytes_written;
    return bytes_written;
}

// Function to handle IOCTL calls
static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    int new_key;
    int old_key = encryption_key;
    switch (cmd) {
        case ENCRYPT:
            if (copy_from_user(&new_key,(int __user*)arg, sizeof(int)))return -EFAULT;
            if (old_key == new_key) return 0;

            // Decrypt existing data using the old key
            encrypt_data(message, message_size);

            encryption_key = new_key;

            // Encrypt existing data using the new key
            encrypt_data(message, message_size);

            printk(KERN_INFO
            "Encryption key updated: %d\n", encryption_key);
            break;

        case GET_INFO: {
            struct device_info info = {
                    .major_number = major_number,
                    .encryption_key = encryption_key
            };

            if (copy_to_user((struct device_info __user*)arg, &info, sizeof(struct device_info)))
            return -EFAULT;

            printk(KERN_INFO
            "Device information sent\n");
        }
            break;

        default:
            return -ENOTTY;  // Invalid IOCTL command
    }

    return 0;
}

// Define file operations for the device
static struct file_operations fops = {
        .open = device_open,
        .release = device_release,
        .read = device_read,
        .write = device_write,
        .unlocked_ioctl = device_ioctl,
};

// Function to initialize the device driver
static int __init

chardev_init(void) {
    // Register the character device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT
        "Failed to register a major number\n");
        return major_number;
    }

    // Create the device class
    chardev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(chardev_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT
        "Failed to create the device class\n");
        return PTR_ERR(chardev_class);
    }

    // Create the device
    chardev_device = device_create(chardev_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(chardev_device)) {
        class_destroy(chardev_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT
        "Failed to create the device\n");
        return PTR_ERR(chardev_device);
    }

    printk(KERN_INFO
    "Device registered successfully with major number %d\n", major_number);
    printk(KERN_INFO
    "Device: %s\n", DEVICE_NAME);
    printk(KERN_INFO
    "Encryption key: %d\n", encryption_key);

    return 0;
}

// Function to cleanup the device driver
static void __exit

chardev_exit(void) {
    device_destroy(chardev_class, MKDEV(major_number, 0));
    class_unregister(chardev_class);
    class_destroy(chardev_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO
    "Device unregistered\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Simple character device driver");
