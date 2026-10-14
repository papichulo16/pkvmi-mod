#ifndef PKVMI_REGS_H
#define PKVMI_REGS_H

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

struct el1_reg_dump {
  unsigned long sctlr_el1;
  unsigned long tcr_el1;
  unsigned long ttbr0_el1;
  unsigned long ttbr1_el1;
  unsigned long mair_el1;
  unsigned long vbar_el1;
  unsigned long esr_el1;
  unsigned long elr_el1;
  unsigned long spsr_el1;
  unsigned long sp_el1;
  unsigned long tpidr_el1;
  unsigned long par_el1;
};

#endif
