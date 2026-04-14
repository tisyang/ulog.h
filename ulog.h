/*
 * Process uLog simple log library
 * Author: TyK <tisyang@gmail.com>
 * License: MIT License
 * Date: 2026-02-06
 *
 *
 */

#ifndef ULOG_MACRO_LOG_H
#define ULOG_MACRO_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

enum uLogLevel {
    ULOG_LL_DEBUG   = 0,
    ULOG_LL_INFO    = 1,
    ULOG_LL_NOTICE  = 2,
    ULOG_LL_WARNING = 3,
    ULOG_LL_ERROR   = 4,
    ULOG_LL__COUNT
};

// short __FILE___ macro
# define ULOG_FILENAME  (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
# define ULOG_FILELINE  __LINE__
# define ULOG_FUNCNAME  __func__

// log output to new file (not affect stdou/stderr)
// return 0 OK, otherwise error
// filepath: NULL => close output to file
int  ulog_tofile(const char *newfile);
char * ulog_getfile(char *buff, size_t sz);
// get ulog current log file bytes
// return 0 OK, otherwise error
// *size storege filesize
int  ulog_size(long *size);
// flush file
void ulog_flush();
// core output function
void ulog_output(int level, const char *file, int line, const char *func, const char *msg);
// core printf function
void ulog_printf(int level, const char *file, int line, const char *func, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));


// set output level
// return 0 OK, otherwise error
int  ulog_set_level(int level);

enum uLogTimeFmt {
    ULOG_TIME_LONGDATE  = 0,
    ULOG_TIME_SHORTDATE = 1,
    ULOG_TIME_TIMEONLY  = 2,
    ULOG_TIME_MONO      = 3,
    ULOG_TIME__COUNT,
};
// set output time fmt
// return 0 OK, otherwise error
int  ulog_set_timefmt(int timefmt);


enum uLogSrcFmt {
    ULOG_SRC_NONE  = 0,
    ULOG_SRC_LONG  = 1,
    ULOG_SRC_SHORT = 2,
    ULOG_SRC_FULL  = 3,
    ULOG_SRC__COUNT,
};
// set output src fmt, file line func
// return 0 OK, otherwise error
int  ulog_set_srcfmt(int srcfmt);

#define ULOG_LEVEL_(level, fmt, ...) \
        ulog_printf(level, ULOG_FILENAME, ULOG_FILELINE, ULOG_FUNCNAME, fmt, ##__VA_ARGS__)

// debug macro
#define ulog_debug(fmt, ...)   ULOG_LEVEL_(ULOG_LL_DEBUG,   fmt, ##__VA_ARGS__)
#define ulog_info(fmt, ...)    ULOG_LEVEL_(ULOG_LL_INFO,    fmt, ##__VA_ARGS__)
#define ulog_notice(fmt, ...)  ULOG_LEVEL_(ULOG_LL_NOTICE,  fmt, ##__VA_ARGS__)
#define ulog_warn(fmt, ...)    ULOG_LEVEL_(ULOG_LL_WARNING, fmt, ##__VA_ARGS__)
#define ulog_error(fmt, ...)   ULOG_LEVEL_(ULOG_LL_ERROR,   fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

# ifdef ULOG_IMPLEMENTATION
#  ifndef ULOG_LINEBUF_MAXSZ
#   define ULOG_LINEBUF_MAXSZ   4096
#  endif
#  include <stdio.h>
#  include <time.h>
#  include <stdarg.h>
#  include <stdatomic.h>
#  include <errno.h>
#  include <string.h>
# ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#  define isatty _isatty
#  define fileno _fileno
# else
#  include <unistd.h>
#  include <sys/syscall.h>
# endif

static unsigned int ulog_get_tid(void) {
# ifdef _WIN32
    return (unsigned int)GetCurrentThreadId();
# else
    static __thread unsigned int cached_tid = 0;
    if (cached_tid == 0) {
        cached_tid = syscall(SYS_gettid);
    }
    return cached_tid;
# endif
}

static const char * const LEVEL_STR[ULOG_LL__COUNT] = {
    [ULOG_LL_DEBUG]     = "[D]",
    [ULOG_LL_INFO]      = "[I]",
    [ULOG_LL_NOTICE]    = "[N]",
    [ULOG_LL_WARNING]   = "[W]",
    [ULOG_LL_ERROR]     = "[E]",
};

static const char* const LEVEL_TERM_COLOR[] = {
    [ULOG_LL_DEBUG]     = "\x1b[34m",
    [ULOG_LL_INFO]      = "\x1b[32m",
    [ULOG_LL_NOTICE]    = "\x1b[35m",
    [ULOG_LL_WARNING]   = "\x1b[33m",
    [ULOG_LL_ERROR]     = "\x1b[31m",
};
#define COLOR_SRC   "\x1b[90m"
#define COLOR_TIME  "\x1b[90m"
#define COLOR_RESET "\x1b[0m"


#define LEVEL_FROM_BITS(bits)   ((bits) & 0xF)
#define LEVEL_TO_BITS(lv)       ((lv) & 0xF)

#define TIMEFMT_FROM_BITS(bits) (((bits) >> 4) & 0xF)
#define TIMEFMT_TO_BITS(fmt)    (((fmt) & 0xF) << 4)

#define SRCFMT_FROM_BITS(bits)  (((bits) >> 8) & 0xF)
#define SRCFMT_TO_BITS(fmt)     (((fmt) & 0xF) << 8)


struct ulog_ctx {
    int     tty_bits;       // tty check:
                            // bit 0: if checked
                            // bit 1: stdout tty
                            // bit 2: stderr tty
    FILE    *fp;            // current fp
    char    path[4096];     // current file path

    time_t  last_sec;           // last format sec
    char    last_time_str[32];  // last time str
    atomic_uint flags;      // atomic bitflags for internal log fmt use
                            // bit [0:3] => log level
                            // bit [4:8] => time format
                            // bit [8:11] => src format
    atomic_flag write_lock; // atomic lock for internal write
};

#define ULOG_DEFAULT_FLAGS \
    (LEVEL_TO_BITS(ULOG_LL_DEBUG) | TIMEFMT_TO_BITS(ULOG_TIME_SHORTDATE) | SRCFMT_TO_BITS(ULOG_SRC_SHORT))

static struct ulog_ctx g_ulog_ctx = {
    .tty_bits = 0,
    .fp = NULL,
    .path = "",
    .flags = ULOG_DEFAULT_FLAGS,
    .write_lock = ATOMIC_FLAG_INIT,
    .last_sec = 0,
};

static inline void ulog_lock(struct ulog_ctx *ctx)
{
    while (atomic_flag_test_and_set_explicit(&ctx->write_lock, memory_order_acquire)) {
#if defined(__i386__) || defined(__x86_64__)
        __asm__ __volatile__("pause");
#elif defined(__arm__) || defined(__aarch64__)
        __asm__ __volatile__("yield");
#elif defined(__riscv__)
        __asm__ __volatile__ ("pause" ::: "memory");
#else
#endif
    }
}

static inline void ulog_unlock(struct ulog_ctx *ctx)
{
    atomic_flag_clear_explicit(&ctx->write_lock, memory_order_release);
}

int  ulog_set_level(int level)
{
    if (level >= 0 && level < ULOG_LL__COUNT) {
        unsigned expected = atomic_load(&g_ulog_ctx.flags);
        unsigned desired;
        unsigned mask = 0xF;
        do {
            desired = (expected & ~mask) | (level & mask);
        } while (!atomic_compare_exchange_weak(&g_ulog_ctx.flags, &expected, desired));
        return 0;
    } else {
        return EINVAL;
    }
}

int  ulog_set_timefmt(int fmt)
{
    if (fmt >= 0 && fmt < ULOG_TIME__COUNT) {
        unsigned expected = atomic_load(&g_ulog_ctx.flags);
        unsigned desired;
        unsigned mask = 0xF0;
        do {
            desired = (expected & ~mask) | (TIMEFMT_TO_BITS(fmt) & mask);
        } while (!atomic_compare_exchange_weak(&g_ulog_ctx.flags, &expected, desired));
        return 0;
    } else {
        return EINVAL;
    }
}

int  ulog_set_srcfmt(int fmt)
{
    if (fmt >= 0 && fmt < ULOG_SRC__COUNT) {
        unsigned expected = atomic_load(&g_ulog_ctx.flags);
        unsigned desired;
        unsigned mask = 0xF00;
        do {
            desired = (expected & ~mask) | (SRCFMT_TO_BITS(fmt) & mask);
        } while (!atomic_compare_exchange_weak(&g_ulog_ctx.flags, &expected, desired));
        return 0;
    } else {
        return EINVAL;
    }
}

int ulog_size(long *size)
{
    long fsize = 0;
    int ret = 0;
    ulog_lock(&g_ulog_ctx);
    if (g_ulog_ctx.fp) {
        fsize = ftell(g_ulog_ctx.fp);
        if (fsize < 0) {
            ret = errno;
        }
    } else {
        ret = ENOENT;
    }
    ulog_unlock(&g_ulog_ctx);
    if (ret == 0 && size) *size = fsize;
    return ret;
}

char* ulog_getfile(char *buff, size_t sz)
{
    char *path = NULL;
    ulog_lock(&g_ulog_ctx);
    if (g_ulog_ctx.fp) {
        int ret = snprintf(buff, sz, "%s", g_ulog_ctx.path);
        if (ret > 0 && ret < sz) {
            path = buff;
        }
    }
    ulog_unlock(&g_ulog_ctx);
    return path;
}

int ulog_tofile(const char *newfile)
{
    int ret = 0;
    ulog_lock(&g_ulog_ctx);

    if (g_ulog_ctx.tty_bits == 0) {
        g_ulog_ctx.tty_bits = 1;
        g_ulog_ctx.tty_bits |= (!!isatty(fileno(stdout))) << 1;
        g_ulog_ctx.tty_bits |= (!!isatty(fileno(stderr))) << 2;
    }

    if (g_ulog_ctx.fp) {
        fflush(g_ulog_ctx.fp);
        fclose(g_ulog_ctx.fp);
        g_ulog_ctx.fp = NULL;
        g_ulog_ctx.path[0] = '\0';
    }

    if (newfile) {
        g_ulog_ctx.fp = fopen(newfile, "ae");
        if (g_ulog_ctx.fp) {
            snprintf(g_ulog_ctx.path, sizeof(g_ulog_ctx.path), "%s", newfile);
        } else {
            ret = errno;
        }
    }

    ulog_unlock(&g_ulog_ctx);
    if (ret) {
        ulog_error("ulog_tofile newfile='%s' failed, %s", newfile, strerror(ret));
    }
    return ret;
}

void ulog_flush()
{
    ulog_lock(&g_ulog_ctx);
    if (g_ulog_ctx.fp) {
        fflush(g_ulog_ctx.fp);
        int fd = fileno(g_ulog_ctx.fp);
        if (fd >= 0 && !isatty(fd)) {
#ifdef _WIN32
            _commit(fd);
#else
            fsync(fd);
#endif
        }
    }
    ulog_unlock(&g_ulog_ctx);
}

static void ulog_gettime_real(struct timespec *ts)
{
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER li;
    // 获取系统时间（100ns为单位）
    GetSystemTimeAsFileTime(&ft);
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    // 偏移量：1601-01-01 到 1970-01-01 的 100ns 数
    unsigned __int64 epoch_offset = 116444736000000000ULL;
    unsigned __int64 now_ticks = li.QuadPart;

    if (now_ticks > epoch_offset) {
        now_ticks -= epoch_offset;
    } else {
        now_ticks = 0;
    }
    // 1 tick = 100ns
    // 1s = 10,000,000 ticks
    ts->tv_sec  = (time_t)(now_ticks / 10000000ULL);
    ts->tv_nsec = (long)((now_ticks % 10000000ULL) * 100);
#else
    clock_gettime(CLOCK_REALTIME, ts);
#endif
}
static void ulog_gettime_mono(struct timespec *ts)
{
#ifdef _WIN32
    // 频率是固定的，static 局部变量初始化在 C11 中不是线程安全的
    // 但在 MSVC 中，static 变量初始化是线程安全的
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    // 计算秒
    ts->tv_sec = (time_t)(counter.QuadPart / freq.QuadPart);
    // 计算纳秒：先取余再乘，防止直接乘导致的溢出
    // 使用 long long 确保中间计算不会溢出
    long long remainder = counter.QuadPart % freq.QuadPart;
    ts->tv_nsec = (long)((remainder * 1000000000LL) / freq.QuadPart);
#else
    clock_gettime(CLOCK_MONOTONIC, ts);
#endif
}

static inline struct tm * ulog_localtime(const time_t *timer, struct tm *tm)
{
#ifdef _WIN32
    if (localtime_s(tm, timer) == 0) {
        return tm;
    }
    return NULL;
#else
    return localtime_r(timer, tm);
#endif
}

static const char * const TIMEFMT_FMTS[ULOG_TIME__COUNT] = {
    [ULOG_TIME_LONGDATE]    = "%Y-%m-%d %H:%M:%S",
    [ULOG_TIME_SHORTDATE]   = "%m/%d %H:%M:%S",
    [ULOG_TIME_TIMEONLY]    = "%H:%M:%S",
    [ULOG_TIME_MONO]        = "<mono>",
};
// core output function
void ulog_output(int level, const char *file, int line, const char *func, const char *msg)
{
    int msg_len = strlen(msg);

    unsigned flags = atomic_load_explicit(&g_ulog_ctx.flags, memory_order_relaxed);
    if (level < LEVEL_FROM_BITS(flags)) return;

    int timefmt = TIMEFMT_FROM_BITS(flags);
    struct timespec ts = {0};
    if (timefmt == ULOG_TIME_MONO) {
        ulog_gettime_mono(&ts);
    } else {
        ulog_gettime_real(&ts);
    }

    int srcfmt = SRCFMT_FROM_BITS(flags);
    char srcbuf[256] = {0};
    file = file ?: "<\?\?\?>";
    func = func ?: "<\?\?\?>";
    char lbuf[16] = "?";
    if (line >= 0) {
        snprintf(lbuf, sizeof(lbuf), "%d", line);
    }
    switch (srcfmt) {
    case ULOG_SRC_FULL:
        snprintf(srcbuf, sizeof(srcbuf), " %s:%s [%u]%s", file, lbuf, ulog_get_tid(), func);
        break;
    case ULOG_SRC_LONG:
        snprintf(srcbuf, sizeof(srcbuf), " %s:%s %s", file, lbuf, func);
        break;
    case ULOG_SRC_SHORT:
        snprintf(srcbuf, sizeof(srcbuf), " %s:%s", file, lbuf);
        break;
    }

    ulog_lock(&g_ulog_ctx);
    if (g_ulog_ctx.tty_bits == 0) {
        g_ulog_ctx.tty_bits = 1;
        g_ulog_ctx.tty_bits |= (!!isatty(fileno(stdout))) << 1;
        g_ulog_ctx.tty_bits |= (!!isatty(fileno(stderr))) << 2;
    }

    if (ts.tv_sec != g_ulog_ctx.last_sec) {
        if (timefmt == ULOG_TIME_MONO) {
            snprintf(g_ulog_ctx.last_time_str, sizeof(g_ulog_ctx.last_time_str), "%lld", (long long)ts.tv_sec);
        } else {
            struct tm tm_info;
            ulog_localtime(&ts.tv_sec, &tm_info);
            strftime(g_ulog_ctx.last_time_str, sizeof(g_ulog_ctx.last_time_str),
                TIMEFMT_FMTS[timefmt], &tm_info);
        }
        g_ulog_ctx.last_sec = ts.tv_sec;
    }

    FILE *fcur = stdout;
    int is_tty = (g_ulog_ctx.tty_bits >> 1) & 1;
    if (is_tty) {
        fputs(COLOR_TIME, fcur);
        fputs(g_ulog_ctx.last_time_str, fcur);
        fprintf(fcur, ".%06ld ", ts.tv_nsec / 1000);
        fputs(LEVEL_TERM_COLOR[level], fcur);
        fputs(LEVEL_STR[level], fcur);
        fputs(COLOR_SRC, fcur);
        fputs(srcbuf, fcur);
        fputs(COLOR_RESET, fcur);
        fputs(": ", fcur);
    } else {
        fputs(g_ulog_ctx.last_time_str, fcur);
        fprintf(fcur, ".%06ld ", ts.tv_nsec / 1000);
        fputs(LEVEL_STR[level], fcur);
        fputs(srcbuf, fcur);
        fputs(": ", fcur);
    }
    fwrite(msg, 1, (msg_len > ULOG_LINEBUF_MAXSZ ? ULOG_LINEBUF_MAXSZ : msg_len), fcur);
    fputc('\n', fcur);

    if (g_ulog_ctx.fp) {
        fcur = g_ulog_ctx.fp;
        fputs(g_ulog_ctx.last_time_str, fcur);
        fprintf(fcur, ".%06ld ", ts.tv_nsec / 1000);
        fputs(LEVEL_STR[level], fcur);
        fputs(srcbuf, fcur);
        fputs(": ", fcur);
        fwrite(msg, 1, (msg_len > ULOG_LINEBUF_MAXSZ ? ULOG_LINEBUF_MAXSZ : msg_len), fcur);
        fputc('\n', fcur);
        if (level >= ULOG_LL_WARNING) fflush(g_ulog_ctx.fp);
    }

    ulog_unlock(&g_ulog_ctx);
}

// core printf function
void ulog_printf(int level, const char *file, int line, const char *func, const char *fmt, ...)
{
    unsigned flags = atomic_load_explicit(&g_ulog_ctx.flags, memory_order_relaxed);
    if (level < LEVEL_FROM_BITS(flags)) return;

    char buff[ULOG_LINEBUF_MAXSZ];
    va_list arglist;
    va_start(arglist, fmt);
    vsnprintf(buff, sizeof(buff), fmt, arglist);
    va_end(arglist);
    ulog_output(level, file, line, func, buff);
}

# endif // ULOG_IMPLEMENTATION

#endif // ULOG_MACRO_LOG_H
