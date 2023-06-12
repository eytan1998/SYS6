#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/time.h>
#include <linux/device.h>

static int major_number;               // Major number assigned to our device driver
#define DEVICE_NAME "chardev"      // Device name to be created
#define CLASS_NAME "chardev_class" // Class name for the device

static struct class *chardev_class;    // Device class
static struct device *chardev_device;  // Device structure

//manage message
#define BUF_LEN 128                  // Maximum length of the message buffer
static char message[BUF_LEN];          // Message buffer to store data
static int message_size = 0;           // Current size of the message buffer
static int message_index = 0;           // to loop

//for key
#define ENCRYPT _IOW('k', 1, int)     // IOCTL command to update the encryption key
static int encryption_key = 0;         // Encryption key
module_param(encryption_key, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(encryption_key, "An integer");

//for /sys/num_bytes_written
static struct kobject *sysfs_kobj;     // Pointer to sysfs kobject
static ssize_t num_bytes_written_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static struct kobj_attribute num_bytes_written_attribute = __ATTR_RO(num_bytes_written);

//lock read and write
static DEFINE_RWLOCK(lock);


#define GET_INFO _IOR('k', 2, int)    // IOCTL command to get device information
struct device_info {
    int major_number;
    int encryption_key;
};

//encrypt by 32 way xor
static void encrypt_data(char *data, size_t length) {
    char *keyBytes = (char *)&encryption_key; // Treat the int as an array of bytes

    for (int j = 0; j < length;) {
        int iter = length - j;
        if (iter > 4)
            iter = 4;
        for (int i = 0; i < iter; ++i) {
            data[j + i] = data[j + i] ^ keyBytes[iter - i - 1];
        }
        j += 4;
    }
}

// Function to open the device
static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Chardev: Device opened\n");
    return 0;
}

// Function to release the device
static int device_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Chardev: Device closed\n");
    return 0;
}

static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset) {
// Function to read data from the device
    int bytes_read;
    unsigned long flags;
    if(message_size ==0) return 0;

    pr_info("Chardev: %d try Lock Read.\n",task_pid_nr(current));
    read_lock_irqsave(&lock, flags);
    pr_info("Chardev: Read Locked by %d.\n",task_pid_nr(current));

    if (*offset >= message_size) return 0; // End of file

    bytes_read = 0;

// Copy data from kernel space to user space
    bytes_read = min(length, (size_t)(message_size - *offset));
    if (copy_to_user(buffer, message + *offset, bytes_read))
    return -EFAULT;

    *offset += bytes_read;

    read_unlock_irqrestore(&lock, flags);
    pr_info("Chardev: Read Unlock by %d.\n",task_pid_nr(current));
    return bytes_read;
}

// Function to write data to the device
static ssize_t device_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset) {
    int bytes_written;
    int available_space;
    int chunk_size;
    unsigned long flags;

    pr_info("Chardev: %d try Lock Write.\n",task_pid_nr(current));
    write_lock_irqsave(&lock, flags);
    pr_info("Chardev: Write Locked by %d.\n",task_pid_nr(current));

    bytes_written = 0;
    // Check if the message buffer has enough space if not loop around
    while(length >0){
        if(message_index >= BUF_LEN){
            // Buffer is full, reset message_size to 0 to override from the beginning
            message_index = 0;
        }

        // Calculate the available space in the buffer
         available_space = BUF_LEN - message_index;

// Determine the number of bytes to copy in this iteration
        chunk_size = (length < available_space) ? length : available_space;

        // Copy data from user space to kernel space
        if (copy_from_user(message + message_index, buffer, chunk_size))
            return -EFAULT;

        encrypt_data(message + message_index, chunk_size);

        bytes_written += chunk_size;
        message_index += chunk_size;
        buffer += chunk_size;
        length -= chunk_size;

    }
    message_size += bytes_written;
    if(message_size > BUF_LEN) message_size = BUF_LEN; // so wont lie

    write_unlock_irqrestore(&lock, flags);
    pr_info("Chardev: %d Unlock Write.\n",task_pid_nr(current));

    return bytes_written;
}

// Function to handle IOCTL calls
static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    unsigned long flags;
    int new_key;
    int old_key = encryption_key;
    switch (cmd) {
        case ENCRYPT:
            if (copy_from_user(&new_key, (int __user *)arg, sizeof(int)))
            return -EFAULT;
            if (old_key == new_key)
                return 0;

            pr_info("Chardev: %d try Lock update key.\n",task_pid_nr(current));
            write_lock_irqsave(&lock, flags);
            pr_info("Chardev: update key Locked by %d.\n",task_pid_nr(current));

            // Decrypt existing data using the old key
            encrypt_data(message, message_size);

            encryption_key = new_key;

            // Encrypt existing data using the new key
            encrypt_data(message, message_size);
            printk(KERN_INFO "Chardev:  Encryption key updated: %d -> %d.\n",old_key, encryption_key);

            write_unlock_irqrestore(&lock, flags);
            pr_info("Chardev: %d Unlock update key.\n",task_pid_nr(current));
            break;

        case GET_INFO: {
            struct device_info info = {
                    .major_number = major_number,
                    .encryption_key = encryption_key
            };

            if (copy_to_user((struct device_info __user *)arg, &info, sizeof(struct device_info)))
            return -EFAULT;

            printk(KERN_INFO "Chardev: Device information sent\n");
        } break;

        default:
            return -ENOTTY; // Invalid IOCTL command
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

// sysfs attribute show function for /sys
static ssize_t num_bytes_written_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return snprintf(buf, PAGE_SIZE, "%d\n", message_size);
}

// Function to initialize the device driver
static int __init chardev_init(void) {
    int sysfs_ret;
    pr_info("Chardev: Try init.\n");

    // Register the character device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Chardev: Failed to register a major number\n");
        return major_number;
    }

    // Create the device class
    chardev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(chardev_class)) {
        //undo unregister_chrdev as all below if failed undo the above
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Chardev: Failed to create the device class\n");
        return PTR_ERR(chardev_class);
    }

    // Create the device
    chardev_device = device_create(chardev_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(chardev_device)) {
        class_destroy(chardev_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Chardev:  Failed to create the device\n");
        return PTR_ERR(chardev_device);
    }

    // Create the sysfs entry
    sysfs_kobj = kobject_create_and_add("sysfs_chardev", kernel_kobj->parent);
    if (!sysfs_kobj) {
        device_destroy(chardev_class, MKDEV(major_number, 0));
        class_unregister(chardev_class);
        class_destroy(chardev_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Chardev:  Failed to create sysfs entry\n");
        return -ENOMEM;
    }

    // Create the sysfs attribute file
    sysfs_ret = sysfs_create_file(sysfs_kobj, &num_bytes_written_attribute.attr);
    if (sysfs_ret) {
        kobject_put(sysfs_kobj);
        device_destroy(chardev_class, MKDEV(major_number, 0));
        class_unregister(chardev_class);
        class_destroy(chardev_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "FChardev:  ailed to create sysfs attribute file\n");
        return sysfs_ret;
    }

    printk(KERN_INFO "Chardev: Device (%s), major (%d) registered successfully and init key to %d\n", DEVICE_NAME,major_number,encryption_key);
    return 0;
}

static void __exit chardev_exit(void) {
    sysfs_remove_file(sysfs_kobj, &num_bytes_written_attribute.attr);
    kobject_put(sysfs_kobj);
    device_destroy(chardev_class, MKDEV(major_number, 0));
    class_unregister(chardev_class);
    class_destroy(chardev_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "Chardev: Device unregistered\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
