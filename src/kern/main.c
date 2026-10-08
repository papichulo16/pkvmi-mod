#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

#include "pkvmi.h"

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

static int __init hello_init(void) {

  int ret = pkvm_driver_init();

  if (ret)
    return ret;

  vm_table_pa_find();

  return misc_register(&g_hello_misc);
}

static void __exit hello_exit(void) {

  misc_deregister(&g_hello_misc);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_DESCRIPTION("hello world from an hvc sent over ioctl");
MODULE_LICENSE("GPL");
