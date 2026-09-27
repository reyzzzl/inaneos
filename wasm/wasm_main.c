#include "fs.h"
#include "part.h"
#include "term_grid.h"

void shell_main(int argc, char **argv);

void enter_shell(void) {
  static char *argv[] = {"shell", 0};
  shell_main(1, argv);
  __builtin_trap(); // no return
}

void kernel_main(void) {
  term_init();
  fs_init();
  part_scan();
  if (part_count() > 0)
    fs_mount_part(0); // automount
  enter_shell();
}
