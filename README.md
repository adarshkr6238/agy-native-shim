# agy-native-shim
Native syscall shim for running Antigravity CLI on Termux without QEMU emulation.

Replaces ~200MB QEMU user-mode emulation with a tiny 70KB LD_PRELOAD shim that intercepts only the missing syscalls on Android kernel 4.9, letting the binary run at full native CPU speed.
