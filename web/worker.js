let wasm = null;
let memory = null;
let disk = null; // disk bytes
let booting = false;
let flag = null; // key flag

const imports = {
  env: {
    memory: null,
    key_wait(s) {
      if (flag) Atomics.wait(flag, 0, s, 1000);
    },
    disk_read(lba, dst) {
      if (!disk) return -1;
      const o = lba * 512;
      if (o + 512 > disk.length) return -1;
      new Uint8Array(memory.buffer).set(disk.subarray(o, o + 512), dst);
      return 0;
    },
    disk_write(lba, src) {
      if (!disk) return -1;
      const o = lba * 512;
      if (o + 512 > disk.length) return -1;
      disk.set(new Uint8Array(memory.buffer).subarray(src, src + 512), o);
      return 0;
    },
    disk_sectors() {
      return disk ? disk.length / 512 : 0;
    },
  },
};

async function boot() {
  if (booting) return;
  booting = true;
  try {
    const [wb, db] = await Promise.all([
      fetch('inaneos.wasm').then((r) => {
        if (!r.ok) throw new Error('wasm http ' + r.status);
        return r.arrayBuffer();
      }),
      fetch('disk.img')
        .then((r) => (r.ok ? r.arrayBuffer() : null))
        .catch(() => null),
    ]);
    disk = db ? new Uint8Array(db) : null;
    memory = new WebAssembly.Memory({ initial: 256, maximum: 4096, shared: true });
    imports.env.memory = memory;
    const { instance } = await WebAssembly.instantiate(wb, imports);
    wasm = instance.exports;
    flag = new Int32Array(memory.buffer, wasm.keyq_ptr() + 8, 1);
    postMessage({ t: 'mem', mem: memory.buffer, cellsPtr: wasm.term_cells(), keyqPtr: wasm.keyq_ptr() });
    postMessage({ t: 'ready', disk: !!disk });
    let enter = () => wasm.kernel_main(); // boot
    for (;;) {
      try {
        enter();
        postMessage({ t: 'error', msg: 'shell returned' });
        return;
      } catch (e) {
        if (!(e instanceof WebAssembly.RuntimeError)) {
          postMessage({ t: 'error', msg: String(e) });
          return;
        }
        const r = wasm.halt_reason_get();
        if (r === 3) {
          enter = () => wasm.enter_shell(); // reentry
          continue;
        }
        if (r === 1) {
          postMessage({ t: 'rebooted' });
          booting = false;
          boot(); // reboot
          return;
        }
        postMessage({ t: 'halted' });
        return;
      }
    }
  } catch (e) {
    postMessage({ t: 'error', msg: String(e) });
  }
  booting = false;
}

onmessage = (e) => {
  if (e.data.t === 'boot') boot();
};
