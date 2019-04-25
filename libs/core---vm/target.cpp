#include "pxt.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <fcntl.h>

//#define HIGH_VOLUME 1

namespace pxt {

void target_exit() {
#ifdef __MINGW32__
    exit(0);
#else
    kill(getpid(), SIGTERM);
#endif
}

extern "C" void target_reset() {
    // TODO
    target_exit();
}

void target_startup() {
}

static FILE *dmesgFile;

static int dmesgPtr;
static int dmesgSerialPtr;
static char dmesgBuf[4096];

void dumpDmesg() {
    auto len = dmesgPtr - dmesgSerialPtr;
    if (len == 0)
        return;
    sendSerial(dmesgBuf + dmesgSerialPtr, len);
    dmesgSerialPtr = dmesgPtr;
}


static void dmesgRaw(const char *buf, uint32_t len) {
    if (!dmesgFile) {
        dmesgFile = fopen("dmesg.txt", "w");
        if (!dmesgFile)
            dmesgFile = stderr;
    }

    if (len > sizeof(dmesgBuf) / 2)
        return;
    if (dmesgPtr + len > sizeof(dmesgBuf)) {
        dmesgPtr = 0;
        dmesgSerialPtr = 0;
    }
    memcpy(dmesgBuf + dmesgPtr, buf, len);
    dmesgPtr += len;
    fwrite(buf, 1, len, dmesgFile);
#ifndef HIGH_VOLUME
    fwrite(buf, 1, len, stderr);
#endif
}

void deepSleep() {
    // nothing to do
}

void dmesg_flush() {
    fflush(dmesgFile);
}

static void dmesgFlushRaw() {
#ifndef HIGH_VOLUME
    dmesg_flush();
#endif
}

void vdmesg(const char *format, va_list arg) {
    char buf[500];

    snprintf(buf, sizeof(buf), "[%8d] ", current_time_ms());
    dmesgRaw(buf, strlen(buf));
    vsnprintf(buf, sizeof(buf), format, arg);
    dmesgRaw(buf, strlen(buf));
    dmesgRaw("\n", 1);

    dmesgFlushRaw();
}

void dmesg(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    vdmesg(format, arg);
    va_end(arg);
}

int getSerialNumber() {
    static int serial;

    if (serial)
        return serial;

    char buf[1024];
    int fd = open("/proc/cpuinfo", O_RDONLY);
    int len = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (len < 0)
        len = 0;
    buf[len] = 0;
    auto p = strstr(buf, "Serial\t");
    if (p) {
        p += 6;
        while (*p && strchr(" \t:", *p))
            p++;
        uint64_t s = 0;
        sscanf(p, "%llu", &s);
        serial = (s >> 32) ^ (s);
    }

    if (!serial)
        serial = 0xf00d0042;

    return serial;
}

} // namespace pxt