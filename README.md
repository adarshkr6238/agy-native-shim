# agy-native-shim

A lightweight native syscall compatibility shim that lets **Antigravity CLI** run on **Termux (Android)** at full native CPU speed — **without QEMU emulation**.

## The Problem

Antigravity CLI ships as a glibc-linked aarch64 binary, but Termux uses Bionic (Android's libc). The default solution uses QEMU user-mode emulation to translate every CPU instruction in software, adding **~5-10x overhead** — making typing, scrolling, and response rendering noticeably sluggish, especially on low-RAM devices.

## The Solution

Since both the host (Termux) and guest (agy binary) are **the same CPU architecture** (aarch64), QEMU's instruction translation is completely unnecessary. The only incompatibility is a handful of Linux syscalls that don't exist on older Android kernels.

This shim replaces the ~200MB QEMU emulator with a **70KB shared library** that:

1. **Lets all CPU instructions run natively** at full hardware speed
2. Only intercepts the ~7 specific syscalls missing on Android kernel 4.9+ (`pidfd_open`, `clone3`, `close_range`, etc.) and returns `ENOSYS` so Go's runtime handles them gracefully
3. Redirects `/etc/resolv.conf` and other path lookups to a fake-root for DNS resolution

## Quick Install

```bash
curl -sL https://github.com/adarshkr6238/agy-native-shim/raw/main/scripts/install.sh | sh
```

## Manual Install

1. Download the shim:
```bash
curl -sL https://github.com/adarshkr6238/agy-native-shim/raw/main/bin/agy_native_shim.so \
  -o ~/.local/lib/agy_native_shim.so
```

2. Download the native launcher:
```bash
curl -sL https://github.com/adarshkr6238/agy-native-shim/raw/main/scripts/agy.native \
  -o ~/antigravity/bin/agy.native
chmod +x ~/antigravity/bin/agy.native
```

3. Swap the launcher:
```bash
cp ~/antigravity/bin/agy ~/antigravity/bin/agy.qemu   # backup
cp ~/antigravity/bin/agy.native ~/antigravity/bin/agy  # swap
```

4. Restart Antigravity.

## Revert to QEMU

```bash
cp ~/antigravity/bin/agy.qemu ~/antigravity/bin/agy
```

## Building from Source

Requires `gcc-aarch64-linux-gnu` (cross-compiler):

```bash
sudo apt-get install gcc-aarch64-linux-gnu
make
```

The compiled shim will be at `bin/agy_native_shim.so`.

## How It Works

The shim is loaded via `LD_PRELOAD` into the Antigravity binary. It intercepts:

| Syscall | Number | Why |
|---------|--------|-----|
| `pidfd_open` | 434 | Not in kernel < 5.3 |
| `clone3` | 435 | Not in kernel < 5.3, Go falls back to `clone` |
| `close_range` | 436 | Not in kernel < 5.9 |
| `openat2` | 437 | Not in kernel < 5.6 |
| `pidfd_getfd` | 438 | Not in kernel < 5.6 |
| `pidfd_send_signal` | 424 | Not in kernel < 5.1 |
| `epoll_pwait2` | 441 | Not in kernel < 5.11 |

It also redirects file opens for `/etc/resolv.conf`, `/etc/hosts`, etc. to a fake-root directory for DNS resolution.

## Requirements

- Termux on Android (aarch64)
- Antigravity CLI installed with glibc support
- Android kernel 4.9+

## License

MIT
