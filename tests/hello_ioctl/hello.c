#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "hello.h"

static long hello_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {

	static const char msg[HELLO_MSG_LEN] = "Hello from EL1!";
	long ret = -ENOTTY;

	if (cmd != HELLO_IOC_GREET)
		goto out;

	ret = copy_to_user((void __user *)arg, msg, sizeof(msg)) ? -EFAULT : 0;
out:
	return ret;
}

static const struct file_operations g_hello_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hello_ioctl,
};

static struct miscdevice g_hello_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hello",
	.fops = &g_hello_fops,
	.mode = 0666,
};

module_misc_device(g_hello_misc);

MODULE_DESCRIPTION("hello world over ioctl");
MODULE_LICENSE("GPL");
