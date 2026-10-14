#include <asm/kvm_pkvm_module.h>
#include <nvhe/trap_handler.h>

#include "el2.h"
#include "pkvmi_regs.h"

#undef memcpy

#define PGD_IDX(va)  (((va) >> L1_SHIFT) & 0x1ffULL)

#define ADDR_MASK_47_12  GENMASK_ULL(47, 12)
#define L1_SHIFT         30UL
#define L1_BLOCK_SIZE    (1UL << L1_SHIFT)
#define L1_ADDR_MASK     GENMASK_ULL(47, 30)
#define L1_NR_ENTRIES    512

#define DESC_VALID       (1UL << 0)   
#define DESC_ATTR_NORMAL (MT_NORMAL << 2)
#define DESC_SH_INNER    (3UL << 8)
#define DESC_AF          (1UL << 10)
#define DESC_XN          (1UL << 54)

static phys_addr_t g_root_pa;
static int g_idx = -1;
static int g_kimage_idx = -1;

static void el2_tlb_flush(void) {

  asm volatile("dsb ish\n\t"
               "tlbi alle2is\n\t"
               "dsb ish\n\t"
               "isb" ::: "memory");
}

static void* el2_tmp_map_phys(phys_addr_t pa) {

  phys_addr_t root_pa = read_sysreg(ttbr0_el2) & ADDR_MASK_47_12;
  unsigned long* root = ops->hyp_va(root_pa);
  void* va = NULL;
  int i;

  for (i = L1_NR_ENTRIES - 1; i >= 0; i--)
    if (!(root[i] & DESC_VALID))
      break;

  if (i < 0)
    goto out;

  root[i] = (pa & L1_ADDR_MASK) | DESC_VALID | DESC_ATTR_NORMAL
          | DESC_SH_INNER | DESC_AF | DESC_XN;

  g_root_pa = root_pa;
  g_idx = i;

  va = (void *) (((unsigned long) i << L1_SHIFT) | (pa & (L1_BLOCK_SIZE - 1)));
  el2_tlb_flush();
out:
  return va;
}

void host_el1_tmp_unmap(void) {

  unsigned long* root;

  if (g_idx < 0)
    return;

  root = ops->hyp_va(g_root_pa);
  root[g_idx] = 0;

  g_idx = -1;

  el2_tlb_flush();
}

// temporarily maps the ttbr1_el1 in the el2 memory space,
// reads the top kbase_pt entry
// maps it into ttbr0_el2 so we can access kernel memory in the pKVM module 
// dont forget to unmap
void* host_el1_map_va(unsigned long va) {

  unsigned long* ttbr0_root = (unsigned long *) ops->hyp_va((phys_addr_t) (read_sysreg(ttbr0_el2) & ADDR_MASK_47_12));
  unsigned long ttbr1_pa = read_sysreg(ttbr1_el1);
  unsigned long* ttbr1_va = el2_tmp_map_phys(ttbr1_pa & ADDR_MASK_47_12);
  unsigned long kbase_pt_pa = 0;

  if (!ttbr1_va)
    return NULL;

  // i lowkey dont understand why its + 1, but when looking at gdb
  // i was one off??? I thought the logic was sound 
  g_kimage_idx = PGD_IDX((unsigned long) kimage_voff) + 1;
  kbase_pt_pa = ttbr1_va[g_kimage_idx];

  host_el1_tmp_unmap();

  // bad map base
  if (ttbr0_root[PGD_IDX(va)] & DESC_VALID)
    return NULL;

  ttbr0_root[PGD_IDX(va)] = kbase_pt_pa;

  return (void *) va;
}

void host_el1_unmap(void) {

  unsigned long* ttbr0_root = (unsigned long *) ops->hyp_va((phys_addr_t) (read_sysreg(ttbr0_el2) & ADDR_MASK_47_12));

  if (g_kimage_idx < 0)
    return;

  ttbr0_root[g_kimage_idx] = 0;
  g_kimage_idx = -1;
}

int host_el1_read_pa(void* dst_page, void* pa) {

  unsigned long kbase_tmp = (((unsigned long) &host_el1_read_pa & (0x1ffUL << L1_SHIFT)) + (1UL << L1_SHIFT));
  unsigned long tmp = (unsigned long) host_el1_map_va(kbase_tmp);

  if (!tmp)
    return -EFAULT;

  ops->memcpy(dst_page, (void *) ((unsigned long) pa + tmp), PAGE_SIZE);

  host_el1_unmap();
  return 0;
}

void host_el1_reg_dump(void* dst_page) {

  struct el1_reg_dump* out = dst_page;

  out->sctlr_el1 = read_sysreg(sctlr_el1);
  out->tcr_el1 = read_sysreg(tcr_el1);
  out->ttbr0_el1 = read_sysreg(ttbr0_el1);
  out->ttbr1_el1 = read_sysreg(ttbr1_el1);
  out->mair_el1 = read_sysreg(mair_el1);
  out->vbar_el1 = read_sysreg(vbar_el1);
  out->esr_el1 = read_sysreg(esr_el1);
  out->elr_el1 = read_sysreg(elr_el1);
  out->spsr_el1 = read_sysreg(spsr_el1);
  out->sp_el1 = read_sysreg(sp_el1);
  out->tpidr_el1 = read_sysreg(tpidr_el1);
  out->par_el1 = read_sysreg(par_el1);
}

