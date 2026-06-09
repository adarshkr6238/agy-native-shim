/*
 * agy_native_shim.c
 * 
 * Lightweight syscall compatibility shim for running glibc aarch64 binaries
 * natively on Android/Termux WITHOUT QEMU emulation.
 *
 * This replaces QEMU user-mode emulation by only intercepting the specific
 * syscalls that are missing or broken on older Android kernels (4.9).
 * Everything else runs at full native CPU speed.
 *
 * Intercepted syscalls:
 *   - pidfd_open (nr 434)     - not in kernel < 5.3
 *   - pidfd_getfd (nr 438)    - not in kernel < 5.6
 *   - pidfd_send_signal (nr 424) - not in kernel < 5.1
 *   - clone3 (nr 435)         - not in kernel < 5.3, Go falls back to clone
 *   - close_range (nr 436)    - not in kernel < 5.9
 *   - openat2 (nr 437)        - not in kernel < 5.6
 *   - epoll_pwait2 (nr 441)   - not in kernel < 5.11
 *   - open/openat             - redirect /etc paths to fake-root
 *
 * Build (cross-compile for aarch64-linux-gnu):
 *   aarch64-linux-gnu-gcc -shared -fPIC -O2 -o agy_native_shim.so agy_native_shim.c -ldl
 *
 * Usage:
 *   LD_PRELOAD=/path/to/agy_native_shim.so /path/to/ld-linux-aarch64.so.1 \
 *     --library-path /path/to/glibc/libs /path/to/agy.va39
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Syscall numbers for aarch64 that may be missing on kernel 4.9 */
#ifndef __NR_pidfd_send_signal
#define __NR_pidfd_send_signal 424
#endif
#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434
#endif
#ifndef __NR_clone3
#define __NR_clone3 435
#endif
#ifndef __NR_close_range
#define __NR_close_range 436
#endif
#ifndef __NR_openat2
#define __NR_openat2 437
#endif
#ifndef __NR_pidfd_getfd
#define __NR_pidfd_getfd 438
#endif
#ifndef __NR_epoll_pwait2
#define __NR_epoll_pwait2 441
#endif

/* Path to fake root - set via AGY_FAKE_ROOT env var or default */
static char fake_root[512] = "";
static int  fake_root_len = 0;
static int  initialized = 0;

/* Paths we need to redirect */
static const char *redirect_paths[] = {
    "/etc/resolv.conf",
    "/etc/hosts",
    "/etc/nsswitch.conf",
    "/etc/ssl/certs",
    "/etc/localtime",
    NULL
};

static void init_shim(void) {
    if (initialized) return;
    initialized = 1;
    
    const char *root = getenv("AGY_FAKE_ROOT");
    if (root && root[0]) {
        strncpy(fake_root, root, sizeof(fake_root) - 1);
        fake_root[sizeof(fake_root) - 1] = '\0';
        fake_root_len = strlen(fake_root);
    }
}

/* Check if a path should be redirected, return new path or NULL */
static const char *redirect_path(const char *path, char *buf, size_t buflen) {
    if (!path || !fake_root_len) return NULL;
    
    for (int i = 0; redirect_paths[i]; i++) {
        if (strcmp(path, redirect_paths[i]) == 0) {
            int written = snprintf(buf, buflen, "%s%s", fake_root, path);
            if (written > 0 && (size_t)written < buflen) {
                /* Only redirect if the file actually exists in fake-root */
                if (access(buf, F_OK) == 0) {
                    return buf;
                }
            }
            return NULL;
        }
    }
    return NULL;
}

/* --- Syscall interception --- */

/* Override the glibc syscall() function */
typedef long (*orig_syscall_fn_t)(long number, ...);

long syscall(long number, ...) {
    static orig_syscall_fn_t orig_syscall = NULL;
    if (!orig_syscall) {
        orig_syscall = (orig_syscall_fn_t)dlsym(RTLD_NEXT, "syscall");
        if (!orig_syscall) {
            errno = ENOSYS;
            return -1;
        }
    }
    
    /* Stub out syscalls not available on kernel 4.9 */
    switch (number) {
        case __NR_pidfd_open:
        case __NR_pidfd_getfd:
        case __NR_pidfd_send_signal:
        case __NR_clone3:
        case __NR_close_range:
        case __NR_openat2:
        case __NR_epoll_pwait2:
            errno = ENOSYS;
            return -1;
        
        default: {
            /* Pass through to real syscall with up to 6 args */
            va_list ap;
            va_start(ap, number);
            long a1 = va_arg(ap, long);
            long a2 = va_arg(ap, long);
            long a3 = va_arg(ap, long);
            long a4 = va_arg(ap, long);
            long a5 = va_arg(ap, long);
            long a6 = va_arg(ap, long);
            va_end(ap);
            return orig_syscall(number, a1, a2, a3, a4, a5, a6);
        }
    }
}

/* --- File path redirection via open/openat interception --- */

typedef int (*orig_open_fn_t)(const char *pathname, int flags, ...);
typedef int (*orig_openat_fn_t)(int dirfd, const char *pathname, int flags, ...);

int open(const char *pathname, int flags, ...) {
    static orig_open_fn_t orig_open = NULL;
    if (!orig_open) {
        orig_open = (orig_open_fn_t)dlsym(RTLD_NEXT, "open");
        init_shim();
    }
    
    char buf[1024];
    const char *actual_path = redirect_path(pathname, buf, sizeof(buf));
    if (actual_path) pathname = actual_path;
    
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode_t mode = va_arg(ap, mode_t);
        va_end(ap);
        return orig_open(pathname, flags, mode);
    }
    return orig_open(pathname, flags);
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    static orig_openat_fn_t orig_openat = NULL;
    if (!orig_openat) {
        orig_openat = (orig_openat_fn_t)dlsym(RTLD_NEXT, "openat");
        init_shim();
    }
    
    char buf[1024];
    const char *actual_path = redirect_path(pathname, buf, sizeof(buf));
    if (actual_path) pathname = actual_path;
    
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode_t mode = va_arg(ap, mode_t);
        va_end(ap);
        return orig_openat(dirfd, pathname, flags, mode);
    }
    return orig_openat(dirfd, pathname, flags);
}

/* Also intercept access() for path redirection */
typedef int (*orig_access_fn_t)(const char *pathname, int mode);

int __xstat(int ver, const char *pathname, void *statbuf) {
    /* Not needed for now, but placeholder */
    typedef int (*orig_fn_t)(int, const char*, void*);
    static orig_fn_t orig = NULL;
    if (!orig) orig = (orig_fn_t)dlsym(RTLD_NEXT, "__xstat");
    return orig(ver, pathname, statbuf);
}
