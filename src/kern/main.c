#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

#include "pkvmi.h"

static const struct file_operations g_pkvmi_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = pkvmi_ioctl,
};

static struct miscdevice g_pkvmi_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "pkvmi",
	.fops = &g_pkvmi_fops,
	.mode = 0666,
};

static int __init pkvmi_init(void) {

  int ret = pkvm_driver_init();

  if (ret)
    return ret;

  vm_table_pa_find();

  return misc_register(&g_pkvmi_misc);
}

static void __exit pkvmi_exit(void) {

  misc_deregister(&g_pkvmi_misc);
}

module_init(pkvmi_init);
module_exit(pkvmi_exit);

MODULE_DESCRIPTION("hello world from an hvc sent over ioctl");
MODULE_LICENSE("GPL");
