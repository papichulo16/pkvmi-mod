#include <asm/kvm_pkvm_module.h>
#include <nvhe/trap_handler.h>

#include "el2.h"
#include "pkvmi_args.h"

struct pkvm_module_ops* ops;

static void** p_vm_table;
static void* vm_table;
void* kimage_voff;

void* shared_page;
static int shared_pfn;

static int find_vm_table_hva(void* vm_table_pa) {

  if (!vm_table_pa)
    return -EFAULT;

  p_vm_table = (void **) ops->hyp_va((phys_addr_t) vm_table_pa);

  if (!p_vm_table) 
    return -EFAULT;

  vm_table = *p_vm_table;

  // page aligned
  if (!vm_table || (unsigned long) vm_table & 0xfff)
    return -EFAULT;

  return 0;
} 

static int share_page(int pfn) {

  int ret = 0;
  void * page;

  if (shared_page)
    return -EFAULT;

  ret = ops->host_share_hyp(pfn);
  if (ret)
    goto end;

  page = ops->hyp_va(pfn << PAGE_SHIFT);
	ret = ops->pin_shared_mem(page, page + PAGE_SIZE);

  if (ret) {
		ops->host_unshare_hyp(pfn);
		goto end;
	}

  shared_page = page;
	shared_pfn = pfn;

end:
  return ret;
}

static int pkvmi_init_globals(void* el1_kbase, void* vm_table_pa, int pfn) {

  int ret = find_vm_table_hva(vm_table_pa);

  if (ret)
    goto end;

  kimage_voff = el1_kbase;

  ret = share_page(pfn);

end:
  return ret;
}

void pkvmi_hvc(struct kvm_cpu_context *ctx) {

  switch (cpu_reg(ctx, 1)) {

    case PKVMI_INIT:

      cpu_reg(ctx, 1) = pkvmi_init_globals((void *) cpu_reg(ctx, 2), (void *) cpu_reg(ctx, 3), cpu_reg(ctx, 4));
      break;

    case PKVMI_REG_DUMP:

      if (!shared_page) {
        cpu_reg(ctx, 1) = 1;
        break;
      }

      host_el2_reg_dump(shared_page);
      cpu_reg(ctx, 1) = 0;
      break;

    case PKVMI_READ_PA:

      if (!shared_page) {
        cpu_reg(ctx, 1) = 1;
        break;
      }

      host_el2_read_pa(shared_page, (void *) cpu_reg(ctx, 2));
      cpu_reg(ctx, 1) = 0;
      break;

    case PKVMI_REG_DUMP_EL1:

      if (!shared_page) {
        cpu_reg(ctx, 1) = 1;
        break;
      }

      host_el1_reg_dump(shared_page);
      cpu_reg(ctx, 1) = 0;
      break;

    case PKVMI_READ_PA_EL1:

      if (!shared_page) {
        cpu_reg(ctx, 1) = 1;
        break;
      }

      cpu_reg(ctx, 1) = host_el1_read_pa(shared_page, (void *) cpu_reg(ctx, 2));
      break;

    default:

      cpu_reg(ctx, 1) = 1;
      break;
  }
}

int pkvmi_init(struct pkvm_module_ops* _ops) {

  ops = _ops;

  return 0;
}

