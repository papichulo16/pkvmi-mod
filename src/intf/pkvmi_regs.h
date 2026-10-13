#ifndef PKVMI_REGS_H
#define PKVMI_REGS_H

/* EL2 writes this at offset 0 of the shared page for PKVMI_REG_DUMP. The
 * kernel and userspace read the same layout back out of the page. */
struct hyp_reg_dump {
  unsigned long hcr_el2;
  unsigned long sctlr_el2;
  unsigned long tcr_el2;
  unsigned long ttbr0_el2;
  unsigned long mair_el2;
  unsigned long vbar_el2;
  unsigned long esr_el2;
  unsigned long elr_el2;
  unsigned long spsr_el2;
  unsigned long tpidr_el2;
  unsigned long vttbr_el2;
  unsigned long vtcr_el2;
};

#endif
