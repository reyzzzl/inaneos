// main kernel
// modul

#include "vga.h"

// TODO: define multiboot_info
// dont guess the offsets
void kernel_main(unsigned int magic, unsigned int mbi_addr) {
    // use multiboot magic
    if (magic != 0x2BADB002) {
        return;
    }

    // TODO: parse mbi_addr
    (void)mbi_addr;

    term_init();
    term_puts("inaneos v0.0.2\n");

    // TODO: idt + pic next
    // TODO: tiny shell
}
