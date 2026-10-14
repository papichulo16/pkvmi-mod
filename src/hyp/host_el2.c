#include <asm/kvm_pkvm_module.h>
#include <nvhe/trap_handler.h>

#include "el2.h"
#include "pkvmi_args.h"
#include "pkvmi_regs.h"

#undef memcpy

void host_el2_reg_dump(void* dst_page) {

  struct hyp_reg_dump* out = dst_page;

  out->hcr_el2 = read_sysreg(hcr_el2);
  out->sctlr_el2 = read_sysreg(sctlr_el2);
  out->tcr_el2 = read_sysreg(tcr_el2);
  out->ttbr0_el2 = read_sysreg(ttbr0_el2);
  out->mair_el2 = read_sysreg(mair_el2);
  out->vbar_el2 = read_sysreg(vbar_el2);
  out->esr_el2 = read_sysreg(esr_el2);
  out->elr_el2 = read_sysreg(elr_el2);
  out->spsr_el2 = read_sysreg(spsr_el2);
  out->tpidr_el2 = read_sysreg(tpidr_el2);
  out->vttbr_el2 = read_sysreg(vttbr_el2);
  out->vtcr_el2 = read_sysreg(vtcr_el2);
}

void host_el2_read_pa(void* dst_page, void* pa) {

  void* src = ops->hyp_va((phys_addr_t) pa);

  ops->memcpy(dst_page, src, PAGE_SIZE);
}
