#include "pkvmi.h"
#include "pkvmi_regs.h"

#define PAGE_SIZE 0x1000

#define SYS_IOCTL 29
#define SYS_OPENAT 56
#define SYS_READ 63
#define SYS_WRITE 64
#define SYS_EXIT_GROUP 94

#define AT_FDCWD (-100)
#define O_RDWR 2

#define LINE_LEN 256

static long sys_call(long nr, long a0, long a1, long a2) {

	register long x8 __asm__("x8") = nr;
	register long x0 __asm__("x0") = a0;
	register long x1 __asm__("x1") = a1;
	register long x2 __asm__("x2") = a2;

	__asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2)
			 : "memory");
	return x0;
}

static long str_len(const char *s) {

	long len = 0;

	while (s[len])
		len++;

	return len;
}

static void print_n(const char *str, long len) {

	sys_call(SYS_WRITE, 1, (long)str, len);
}

static void print(const char *str) {

	print_n(str, str_len(str));
}

static void print_u64_hex(unsigned long v) {

	char buf[18];
	int i;

	buf[0] = '0';
	buf[1] = 'x';

	for (i = 0; i < 16; i++) {
		int nib = (v >> ((15 - i) * 4)) & 0xf;
		buf[2 + i] = nib < 10 ? '0' + nib : 'a' + nib - 10;
	}

	print_n(buf, sizeof(buf));
}

static void print_byte_hex(unsigned char b) {

	char buf[2];
	int hi = b >> 4;
	int lo = b & 0xf;

	buf[0] = hi < 10 ? '0' + hi : 'a' + hi - 10;
	buf[1] = lo < 10 ? '0' + lo : 'a' + lo - 10;

	print_n(buf, sizeof(buf));
}

static const char *skip_spaces(const char *s) {

	while (*s == ' ' || *s == '\t')
		s++;

	return s;
}

static int tok_eq(const char *line, const char *cmd, const char **rest) {

	const char *l = skip_spaces(line);

	while (*cmd) {
		if (*l != *cmd)
			return 0;
		l++;
		cmd++;
	}

	if (*l != 0 && *l != ' ' && *l != '\t')
		return 0;

	*rest = skip_spaces(l);

	return 1;
}

static unsigned long parse_hex(const char *s, int *ok) {

	unsigned long v = 0;
	int any = 0;

	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		s += 2;

	while (*s) {
		char c = *s;
		int d;

		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'f')
			d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			d = c - 'A' + 10;
		else
			break;

		v = (v << 4) | d;
		any = 1;
		s++;
	}

	*ok = any;

	return v;
}

static long read_line(char *buf, long max) {

	long len = 0;

	while (len < max - 1) {
		char c;
		long r = sys_call(SYS_READ, 0, (long)&c, 1);

		if (r <= 0)
			return len == 0 ? -1 : len;

		if (c == '\n')
			break;

		buf[len++] = c;
	}

	buf[len] = 0;

	return len;
}

static void cmd_regdump(int fd) {

	char page[IOCTL_MSG_LEN];
	struct hyp_reg_dump *r = (struct hyp_reg_dump *)page;

	if (sys_call(SYS_IOCTL, fd, HYPM_REG_DUMP, (long)page) != 0) {
		print("regdump failed\n");
		return;
	}

	print("hcr_el2   "); print_u64_hex(r->hcr_el2);   print("\n");
	print("sctlr_el2 "); print_u64_hex(r->sctlr_el2); print("\n");
	print("tcr_el2   "); print_u64_hex(r->tcr_el2);   print("\n");
	print("ttbr0_el2 "); print_u64_hex(r->ttbr0_el2); print("\n");
	print("mair_el2  "); print_u64_hex(r->mair_el2);  print("\n");
	print("vbar_el2  "); print_u64_hex(r->vbar_el2);  print("\n");
	print("esr_el2   "); print_u64_hex(r->esr_el2);   print("\n");
	print("elr_el2   "); print_u64_hex(r->elr_el2);   print("\n");
	print("spsr_el2  "); print_u64_hex(r->spsr_el2);  print("\n");
	print("tpidr_el2 "); print_u64_hex(r->tpidr_el2); print("\n");
	print("vttbr_el2 "); print_u64_hex(r->vttbr_el2); print("\n");
	print("vtcr_el2  "); print_u64_hex(r->vtcr_el2);  print("\n");
}

static void cmd_regdump_el1(int fd) {

	char page[IOCTL_MSG_LEN];
	struct el1_reg_dump *r = (struct el1_reg_dump *)page;

	if (sys_call(SYS_IOCTL, fd, HYPM_REG_DUMP_EL1, (long)page) != 0) {
		print("regdump_el1 failed\n");
		return;
	}

	print("sctlr_el1 "); print_u64_hex(r->sctlr_el1); print("\n");
	print("tcr_el1   "); print_u64_hex(r->tcr_el1);   print("\n");
	print("ttbr0_el1 "); print_u64_hex(r->ttbr0_el1); print("\n");
	print("ttbr1_el1 "); print_u64_hex(r->ttbr1_el1); print("\n");
	print("mair_el1  "); print_u64_hex(r->mair_el1);  print("\n");
	print("vbar_el1  "); print_u64_hex(r->vbar_el1);  print("\n");
	print("esr_el1   "); print_u64_hex(r->esr_el1);   print("\n");
	print("elr_el1   "); print_u64_hex(r->elr_el1);   print("\n");
	print("spsr_el1  "); print_u64_hex(r->spsr_el1);  print("\n");
	print("sp_el1    "); print_u64_hex(r->sp_el1);    print("\n");
	print("tpidr_el1 "); print_u64_hex(r->tpidr_el1); print("\n");
	print("par_el1   "); print_u64_hex(r->par_el1);   print("\n");
}

static void cmd_readpa_el1(int fd, const char *arg) {

	char page[IOCTL_MSG_LEN];
	int ok;
	unsigned long va;
	int row;

	va = parse_hex(arg, &ok);

	if (!ok) {
		print("usage: readpa_el1 <hex-va>\n");
		return;
	}

	*(unsigned long *)page = va;

	if (sys_call(SYS_IOCTL, fd, HYPM_READ_PA_EL1, (long)page) != 0) {
		print("readpa_el1 failed\n");
		return;
	}

	for (row = 0; row < (int) (IOCTL_MSG_LEN / 0x10); row++) {
		int col;

		print_u64_hex(va + row * 16);
		print("  ");

		for (col = 0; col < 2; col++) {
			print_u64_hex(((unsigned long *) page)[row * 2 + col]);
			print(" ");
		}

		print("\n");
	}
}

static void cmd_readpa(int fd, const char *arg) {

	char page[IOCTL_MSG_LEN];
	int ok;
	unsigned long pa;
	int row;

	pa = parse_hex(arg, &ok);

	if (!ok) {
		print("usage: readpa <hex-pa>\n");
		return;
	}

	*(unsigned long *)page = pa;

	if (sys_call(SYS_IOCTL, fd, HYPM_READ_PA, (long)page) != 0) {
		print("readpa failed\n");
		return;
	}

	for (row = 0; row < (int) (IOCTL_MSG_LEN / 0x10); row++) {
		int col;

		print_u64_hex(pa + row * 16);
		print("  ");

		for (col = 0; col < 2; col++) {
			print_u64_hex(((unsigned long *) page)[row * 2 + col]);
			print(" ");
		}

		print("\n");
	}
}

static void cmd_help(void) {

	print("commands:\n");
	print("  regdump             dump EL2 system registers\n");
	print("  readpa <hex-pa>     read a physical page\n");
	print("  regdump_el1         dump EL1 system registers\n");
	print("  readpa_el1 <hex-va> read a kernel virtual page\n");
	print("  help                show this\n");
	print("  exit                quit\n");
}

void __attribute__((noreturn)) _start(void) {

	char line[LINE_LEN];
	char msg[IOCTL_MSG_LEN];
	long fd = sys_call(SYS_OPENAT, AT_FDCWD, (long)"/dev/pkvmi", O_RDWR);

	if (fd < 0) {
		print("cannot open /dev/pkvmi\n");
		sys_call(SYS_EXIT_GROUP, 1, 0, 0);
		for (;;);
	}

	if (sys_call(SYS_IOCTL, fd, HYPM_INIT, (long)msg) != 0)
		print("init failed\n");
	else
		print("pkvmi init: ");

	print(msg);
	print("\n");

	for (;;) {
		const char *rest;
		long len;

		print("pkvmi> ");

		len = read_line(line, LINE_LEN);

		if (len < 0)
			break;

		if (tok_eq(line, "regdump_el1", &rest))
			cmd_regdump_el1((int)fd);
		else if (tok_eq(line, "readpa_el1", &rest))
			cmd_readpa_el1((int)fd, rest);
		else if (tok_eq(line, "regdump", &rest))
			cmd_regdump((int)fd);
		else if (tok_eq(line, "readpa", &rest))
			cmd_readpa((int)fd, rest);
		else if (tok_eq(line, "help", &rest))
			cmd_help();
		else if (tok_eq(line, "exit", &rest) || tok_eq(line, "quit", &rest))
			break;
		else if (*skip_spaces(line))
			print("unknown command (try 'help')\n");
	}

	print("bye\n");
	sys_call(SYS_EXIT_GROUP, 0, 0, 0);
	for (;;);
}

