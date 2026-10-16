#include <asm/kvm_pkvm_module.h>
#include <nvhe/trap_handler.h>

#include "el2.h"
#include "pkvmi_args.h"
#include "pkvmi_err.h"

struct pkvm_module_ops* ops;

static void** p_vm_table;
static void* vm_table;
void* kimage_voff;

void* shared_page;
static u64 shared_pfn;

static unsigned long find_vm_table_hva(void* vm_table_pa) {

  unsigned long ret = PKVMI_ERR(PKVMI_ERR_VM_TABLE_PA, -EFAULT);

  if (!vm_table_pa)
    goto end;

  p_vm_table = (void **) ops->hyp_va((phys_addr_t) vm_table_pa);
  ret = PKVMI_ERR(PKVMI_ERR_VM_TABLE_HVA, -EFAULT);

  if (!p_vm_table)
    goto end;

  vm_table = *p_vm_table;
  ret = PKVMI_ERR(PKVMI_ERR_VM_TABLE_PTR, -EFAULT);

  // page aligned
  if (!vm_table || (unsigned long) vm_table & 0xfff)
    goto end;

  ret = PKVMI_OK;

end:
  return ret;
}

static unsigned long share_page(u64 pfn) {

  unsigned long ret = PKVMI_ERR(PKVMI_ERR_SHARE_BUSY, -EBUSY);
  void * page;
  int err;

  if (shared_page)
    goto end;

  err = ops->host_share_hyp(pfn);
  ret = PKVMI_ERR(PKVMI_ERR_HOST_SHARE, err);

  if (err)
    goto end;

  page = ops->hyp_va((phys_addr_t) pfn << PAGE_SHIFT);
	err = ops->pin_shared_mem(page, page + PAGE_SIZE);
  ret = PKVMI_ERR(PKVMI_ERR_PIN_SHARED, err);

  if (err) {
		ops->host_unshare_hyp(pfn);
		goto end;
	}

  shared_page = page;
	shared_pfn = pfn;
  ret = PKVMI_OK;

end:
  return ret;
}

static unsigned long pkvmi_init_globals(void* el1_kbase, void* vm_table_pa, u64 pfn) {

  unsigned long ret = find_vm_table_hva(vm_table_pa);

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

