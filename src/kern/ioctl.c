#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <asm/kvm_pkvm_module.h>

#include "pkvmi.h"
#include "pkvmi_args.h"

int __kvm_nvhe_pkvmi_init(const struct pkvm_module_ops *ops);
void __kvm_nvhe_pkvmi_hvc(struct kvm_cpu_context *ctx);

char* g_msg;
static int pkvmi_hvc;
static struct page *shared_page;

int pkvm_driver_init(void) {

  unsigned long token;
  int ret;

  ret = pkvm_load_el2_module(__kvm_nvhe_pkvmi_init, &token);

  if (ret)
    return ret;

  ret = pkvm_register_el2_mod_call(__kvm_nvhe_pkvmi_hvc, token);

  if (ret < 0)
    return ret;

  pkvmi_hvc = ret;

  return 0;
}

static int pkvmi_init_hvc(void) {

  shared_page = alloc_page(GFP_KERNEL | __GFP_ZERO);

	if (!shared_page)
		return -ENOMEM;

  return pkvm_el2_mod_call(pkvmi_hvc, PKVMI_INIT, pkvm_vm_table_pa, page_to_pfn(shared_page));
}

static int pkvmi_reg_dump_hvc(void) {

  if (!shared_page)
    return -ENODEV;

  return pkvm_el2_mod_call(pkvmi_hvc, PKVMI_REG_DUMP);
}

static int pkvmi_read_pa_hvc(unsigned long pa) {

  if (!shared_page)
    return -ENODEV;

  return pkvm_el2_mod_call(pkvmi_hvc, PKVMI_READ_PA, pa);
}

static long pkvmi_page_to_user(unsigned long arg) {

  return copy_to_user((void __user *)arg, page_address(shared_page), IOCTL_MSG_LEN) ? -EFAULT : 0;
}

long pkvmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {

	long ret = -ENOTTY;
  int r;
  unsigned long pa;

  switch (cmd) {

    case HYPM_INIT:

      r = pkvmi_init_hvc();

      if (!r)
        g_msg = "lets goo jit";
      else
        g_msg = "bruh";

      ret = copy_to_user((void __user *)arg, g_msg, strlen(g_msg) + 1) ? -EFAULT : 0;

      break;

    case HYPM_REG_DUMP:

      r = pkvmi_reg_dump_hvc();

      ret = r ? -EIO : pkvmi_page_to_user(arg);

      break;

    case HYPM_READ_PA:

      if (copy_from_user(&pa, (void __user *)arg, sizeof(pa))) {
        ret = -EFAULT;
        break;
      }

      r = pkvmi_read_pa_hvc(pa);

      ret = r ? -EIO : pkvmi_page_to_user(arg);

      break;
  }

	return ret;
}

