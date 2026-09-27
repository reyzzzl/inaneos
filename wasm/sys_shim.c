#include "syscall.h"
#include "fs.h"
#include "fat.h"
#include "part.h"
#include "term_grid.h"

extern void key_wait(int seq); // Atomics.wait

// key queue
#define KEY_N 64
typedef struct {
  int head;       // +0 UI
  int tail;       // +4 wasm
  int seq;        // +8 seq
  int buf[KEY_N]; // +12
} keyq_t;
static keyq_t keyq;
_Static_assert(__builtin_offsetof(keyq_t, tail) == 4, "keyq layout");
_Static_assert(__builtin_offsetof(keyq_t, seq) == 8, "keyq layout");
_Static_assert(__builtin_offsetof(keyq_t, buf) == 12, "keyq layout");
keyq_t *keyq_ptr(void) { return &keyq; }

void shell_main(int argc, char **argv);
void calc_main(int argc, char **argv);
void keo_main(int argc, char **argv);

// halt codes
#define HALT_REBOOT 1
#define HALT_POWEROFF 2
#define HALT_EXIT 3
static volatile int halt_reason;
int halt_reason_get(void) { return halt_reason; }
static void halt_trap(int r) {
  halt_reason = r;
  __builtin_trap();
}

#define MEM_FREE_KB 126976UL
#define MEM_TOTAL_KB 131072UL

#define E_INVAL -22
#define SYS_IO_MAX 4096

static int str_same(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

static long copy_str(char *dst, const char *src, unsigned long cap) {
  unsigned long n = 0;
  if (!src || cap == 0)
    return E_INVAL;
  while (n + 1 < cap && src[n]) {
    dst[n] = src[n];
    n++;
  }
  if (src[n])
    return E_INVAL;
  dst[n] = '\0';
  return (long)n;
}

static unsigned long emit_str(char *buf, unsigned long pos, unsigned long cap,
                              const char *s) {
  while (*s) {
    if (pos >= cap)
      return (unsigned long)-1;
    buf[pos++] = *s++;
  }
  return pos;
}

static unsigned long emit_dec(char *buf, unsigned long pos, unsigned long cap,
                              unsigned long v) {
  char tmp[20];
  int i = 0;
  if (!v)
    tmp[i++] = '0';
  while (v) {
    tmp[i++] = (char)('0' + (v % 10));
    v /= 10;
  }
  while (i--) {
    if (pos >= cap)
      return (unsigned long)-1;
    buf[pos++] = tmp[i];
  }
  return pos;
}

static unsigned long emit_hex(char *buf, unsigned long pos, unsigned long cap,
                              unsigned int v) {
  const char *h = "0123456789ABCDEF";
  if (pos + 2 > cap)
    return (unsigned long)-1;
  buf[pos++] = h[(v >> 4) & 0xF];
  buf[pos++] = h[v & 0xF];
  return pos;
}

long sys_puts(const char *s) {
  unsigned long n = 0;
  if (!s)
    return E_INVAL;
  while (n < SYS_IO_MAX && s[n])
    n++;
  if (n == SYS_IO_MAX)
    return E_INVAL;
  term_write(s, n);
  return 0;
}

long sys_write(const char *s, unsigned long len) {
  if (!s || len > SYS_IO_MAX)
    return E_INVAL;
  term_write(s, len);
  return (long)len;
}

long sys_getc(void) {
  for (;;) {
    int t = __atomic_load_n(&keyq.tail, __ATOMIC_RELAXED);
    int h = __atomic_load_n(&keyq.head, __ATOMIC_ACQUIRE);
    if (t != h) {
      int c = keyq.buf[t];
      __atomic_store_n(&keyq.tail, (t + 1) % KEY_N, __ATOMIC_RELEASE);
      return (long)c;
    }
    int s = __atomic_load_n(&keyq.seq, __ATOMIC_RELAXED);
    t = __atomic_load_n(&keyq.tail, __ATOMIC_RELAXED);
    h = __atomic_load_n(&keyq.head, __ATOMIC_ACQUIRE);
    if (t != h)
      continue;
    key_wait(s);
  }
}

long sys_info(void) {
  term_puts("inaneos beta 0.0.2\n");
  return 0;
}

long sys_reboot(void) { halt_trap(HALT_REBOOT); }

long sys_poweroff(void) {
  term_puts("safe to power off\n");
  halt_trap(HALT_POWEROFF);
}

long sys_exit(void) {
  halt_trap(HALT_EXIT);
}

long sys_run(const char *name, const char *arg) {
  static char nm[32];
  static char ag[128];
  void (*fn)(int, char **) = 0;
  char *argv[3];
  int argc = 1;
  if (copy_str(nm, name, sizeof(nm)) < 0)
    return E_INVAL;
  if (arg && arg[0]) {
    if (copy_str(ag, arg, sizeof(ag)) < 0)
      return E_INVAL;
    argc = 2;
  }
  if (str_same(nm, "shell"))
    fn = shell_main;
  else if (str_same(nm, "calc"))
    fn = calc_main;
  else if (str_same(nm, "keo"))
    fn = keo_main;
  else
    return E_INVAL;
  argv[0] = nm;
  argv[1] = argc > 1 ? ag : 0;
  argv[2] = 0;
  fn(argc, argv);
  return 0;
}

long sys_lsmod(char *buf, unsigned long cap) {
  // grub order
  static const char *mods[] = {"shell", "calc", "keo"};
  unsigned long pos = 0;
  if (!buf || cap > 256)
    return E_INVAL;
  for (int i = 0; i < 3; i++) {
    pos = emit_str(buf, pos, cap, mods[i]);
    pos = emit_str(buf, pos, cap, "\n");
    if (pos == (unsigned long)-1)
      break;
  }
  return pos == (unsigned long)-1 ? E_INVAL : (long)pos;
}

long sys_partlist(char *buf, unsigned long cap) {
  unsigned long pos = 0;
  if (!buf || cap > 512)
    return E_INVAL;
  for (int i = 0; i < part_count(); i++) {
    const part_t *p = part_get(i);
    pos = emit_dec(buf, pos, cap, (unsigned long)i);
    pos = emit_str(buf, pos, cap, i == fs_mounted_idx() ? "* " : " ");
    pos = emit_hex(buf, pos, cap, p->type);
    pos = emit_str(buf, pos, cap, " ");
    pos = emit_dec(buf, pos, cap, p->lba);
    pos = emit_str(buf, pos, cap, " ");
    pos = emit_dec(buf, pos, cap, p->sectors);
    pos = emit_str(buf, pos, cap, "\n");
    if (pos == (unsigned long)-1)
      break;
  }
  return pos == (unsigned long)-1 ? E_INVAL : (long)pos;
}

long sys_mount(long idx) {
  if (idx < 0) {
    fs_unmount();
    return 0;
  }
  return (long)fs_mount_part((int)idx);
}

long sys_fread(const char *p, char *buf, unsigned long cap) {
  static char path[128];
  if (copy_str(path, p, sizeof(path)) < 0 || !buf || cap > 8192)
    return E_INVAL;
  if (fat_mounted())
    return (long)fat_read(path, buf, cap);
  return (long)fs_fread_ram(path, buf, cap);
}

long sys_goto(int row, int col) {
  term_goto(row, col);
  return 0;
}

long sys_fwrite(const char *p, const char *buf, unsigned long len) {
  static char path[128];
  if (copy_str(path, p, sizeof(path)) < 0 || !buf || len > FILE_MAX)
    return E_INVAL;
  return (long)fs_fwrite(path, buf, len);
}

long sys_chdir(const char *p) {
  static char path[128];
  long r = copy_str(path, p, sizeof(path));
  return r < 0 ? r : (long)fs_chdir(path);
}

long sys_mkdir(const char *p) {
  static char path[128];
  long r = copy_str(path, p, sizeof(path));
  return r < 0 ? r : (long)fs_mkdir(path);
}

long sys_listdir(const char *p, char *buf, unsigned long cap) {
  static char path[128];
  long r = copy_str(path, p, sizeof(path));
  if (r < 0 || !buf || cap > 8192)
    return E_INVAL;
  return (long)fs_listdir(path, buf, cap);
}

long sys_getcwd(char *buf, unsigned long cap) {
  if (!buf || cap > 512 || cap < 2)
    return E_INVAL;
  return (long)fs_getcwd(buf, cap);
}

long sys_setcolor(long fg, long bg) {
  term_set_color((int)fg, (int)bg);
  return 0;
}

long sys_meminfo(char *buf, unsigned long cap) {
  unsigned long pos = 0;
  if (!buf || cap > 256 || cap < 32)
    return E_INVAL;
  pos = emit_str(buf, pos, cap, "free: ");
  pos = emit_dec(buf, pos, cap, MEM_FREE_KB);
  pos = emit_str(buf, pos, cap, " KB total: ");
  pos = emit_dec(buf, pos, cap, MEM_TOTAL_KB);
  pos = emit_str(buf, pos, cap, " KB\n");
  return pos == (unsigned long)-1 ? E_INVAL : (long)pos;
}
