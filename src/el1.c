#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <asm/kvm_pkvm_module.h>

#include "hello_el1.h"

int __kvm_nvhe_pkvm_hello_init(const struct pkvm_module_ops *ops);
void __kvm_nvhe_pkvm_hello_hvc(struct kvm_cpu_context *ctx);

char* g_msg;
static int hello_hvc;

static int pkvm_driver_init(void) {

  unsigned long token;
  int ret;

  ret = pkvm_load_el2_module(__kvm_nvhe_pkvm_hello_init, &token);

  if (ret)
    return ret;

  ret = pkvm_register_el2_mod_call(__kvm_nvhe_pkvm_hello_hvc, token);

  if (ret < 0)
    return ret;

  hello_hvc = ret;

  return 0;
}

static long hello_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {

	long ret = -ENOTTY;
  int r;

  switch (cmd) {

    case HELLO_HYPM_GREET:

      r = pkvm_el2_mod_call(hello_hvc);

      if (r == 67)
        g_msg = "Hello from EL2!";
      else 
        g_msg = "bruh";

      ret = copy_to_user((void __user *)arg, g_msg, strlen(g_msg) + 1) ? -EFAULT : 0;

      break;
  }

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

static int __init hello_init(void) {

  int ret = pkvm_driver_init();

  if (ret)
    return ret;

  return misc_register(&g_hello_misc);
}

static void __exit hello_exit(void) {

  misc_deregister(&g_hello_misc);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_DESCRIPTION("hello world from an hvc sent over ioctl");
MODULE_LICENSE("GPL");
