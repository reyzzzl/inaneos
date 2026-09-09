// main kernel
// modul

struct __attribute__((packed)) multiboot_header {
    unsigned int magic;
    unsigned int flags;
    unsigned int checksum;
};

__attribute__((section(".multiboot"), used, aligned(4)))
const struct multiboot_header mb = {
    0x1BADB002,
    0x00000003,
    -(0x1BADB002 + 0x00000003)
};

__attribute__((aligned(16)))
unsigned char stack[16384];

// TODO: move this to a proper boot asm file later
void kernel_main(void);

__attribute__((naked))
void _start(void) {
    __asm__ volatile(
        "cli\n\t"
        "movl $stack+16384,%esp\n\t"
        "call kernel_main\n"
        "1: hlt\n\t"
        "jmp 1b"
    );
}

void kernel_main(void) {
    // FIXME: hardcoded vga address, breaks on serial-only machines
    volatile char *vga = (volatile char *)0xB8000;
    const char *msg = "<<>>";

    for (int i = 0; msg[i] != '\0'; i++) {
        vga[i * 2] = msg[i];
        vga[i * 2 + 1] = 0x0f;
    }
}
