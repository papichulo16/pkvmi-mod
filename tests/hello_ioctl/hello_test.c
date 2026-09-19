#include "hello.h"

#define SYS_IOCTL 29
#define SYS_OPENAT 56
#define SYS_WRITE 64
#define SYS_EXIT_GROUP 94

#define AT_FDCWD (-100)
#define O_RDWR 2

static long sys_call(long nr, long a0, long a1, long a2) {

	register long x8 __asm__("x8") = nr;
	register long x0 __asm__("x0") = a0;
	register long x1 __asm__("x1") = a1;
	register long x2 __asm__("x2") = a2;

	__asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2)
			 : "memory");
	return x0;
}

static int str_eq(const char *a, const char *b) {

	while (*a && *a == *b) {
		a++;
		b++;
	}

	return *a == *b;
}

static void print(const char *str) {

	long len = 0;

	while (str[len])
		len++;

	sys_call(SYS_WRITE, 1, (long)str, len);
}

void __attribute__((noreturn)) _start(void) {

	char msg[HELLO_MSG_LEN];
	int failed = 1;
	long fd = sys_call(SYS_OPENAT, AT_FDCWD, (long)"/dev/hello", O_RDWR);

	if (fd < 0 || sys_call(SYS_IOCTL, fd, HELLO_IOC_GREET, (long)msg) != 0)
		goto out;

	print("kernel says: ");
	print(msg);
	print("\n");

	failed = !str_eq(msg, "Hello from EL1!");
out:
	print(failed ? "FAIL\n" : "PASS\n");
	sys_call(SYS_EXIT_GROUP, failed, 0, 0);
	for (;;)
		;
}
