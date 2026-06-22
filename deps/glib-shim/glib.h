/*
    Minimal glib shim for building libslirp without a real glib dependency.

    Only the subset of glib that libslirp references is provided. Exec/guestfwd
    forwarding (the g_spawn and g_shell family) is stubbed to fail since iris
    does not use it. This is NOT a general-purpose glib replacement.
*/
#ifndef GLIB_SHIM_H
#define GLIB_SHIM_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int gboolean;
typedef char gchar;
typedef unsigned char guchar;
typedef int gint;
typedef unsigned int guint;
typedef int8_t gint8;
typedef uint8_t guint8;
typedef int16_t gint16;
typedef uint16_t guint16;
typedef int32_t gint32;
typedef uint32_t guint32;
typedef int64_t gint64;
typedef uint64_t guint64;
typedef void* gpointer;
typedef const void* gconstpointer;
typedef size_t gsize;
typedef ptrdiff_t gssize;
typedef double gdouble;
typedef float gfloat;
typedef long glong;
typedef unsigned long gulong;
typedef char** GStrv;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define GLIB_MAJOR_VERSION 2
#define GLIB_MINOR_VERSION 78
#define GLIB_MICRO_VERSION 0
#define GLIB_CHECK_VERSION(major, minor, micro)                          \
    (GLIB_MAJOR_VERSION > (major) ||                                     \
     (GLIB_MAJOR_VERSION == (major) && GLIB_MINOR_VERSION > (minor)) ||  \
     (GLIB_MAJOR_VERSION == (major) && GLIB_MINOR_VERSION == (minor) &&  \
      GLIB_MICRO_VERSION >= (micro)))

#if defined(__GNUC__) || defined(__clang__)
#define G_GNUC_PRINTF(a, b) __attribute__((format(printf, a, b)))
#define G_GNUC_UNUSED __attribute__((unused))
#define G_NORETURN __attribute__((noreturn))
#define G_LIKELY(x) __builtin_expect(!!(x), 1)
#define G_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define G_GNUC_PRINTF(a, b)
#define G_GNUC_UNUSED
#define G_NORETURN
#define G_LIKELY(x) (x)
#define G_UNLIKELY(x) (x)
#endif

#define G_N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))
#define G_STATIC_ASSERT(expr) typedef char g_static_assert_dummy[(expr) ? 1 : -1] G_GNUC_UNUSED
#define G_STRFUNC ((const char*)(__func__))

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/* iris only targets little-endian hosts. libslirp gates header bitfield order
   and ntoh/hton macros on these, so they MUST be defined correctly. */
#define G_LITTLE_ENDIAN 1234
#define G_BIG_ENDIAN 4321
#define G_PDP_ENDIAN 3412
#define G_BYTE_ORDER G_LITTLE_ENDIAN

/* iris only targets little-endian hosts, so big-endian conversions byteswap. */
#define G_BSWAP16(v) \
    ((guint16)((((guint16)(v) & 0x00ffu) << 8) | (((guint16)(v) & 0xff00u) >> 8)))
#define G_BSWAP32(v)                              \
    ((guint32)((((guint32)(v) & 0x000000ffu) << 24) | \
               (((guint32)(v) & 0x0000ff00u) << 8) |  \
               (((guint32)(v) & 0x00ff0000u) >> 8) |  \
               (((guint32)(v) & 0xff000000u) >> 24)))
#define GUINT16_TO_BE(v) G_BSWAP16(v)
#define GUINT16_FROM_BE(v) G_BSWAP16(v)
#define GUINT32_TO_BE(v) G_BSWAP32(v)
#define GUINT32_FROM_BE(v) G_BSWAP32(v)
#define GINT16_TO_BE(v) ((gint16)G_BSWAP16(v))
#define GINT16_FROM_BE(v) ((gint16)G_BSWAP16(v))
#define GINT32_TO_BE(v) ((gint32)G_BSWAP32(v))
#define GINT32_FROM_BE(v) ((gint32)G_BSWAP32(v))

void g_shim_log(const char* level, const char* fmt, ...) G_GNUC_PRINTF(2, 3);
G_NORETURN void g_shim_fatal(const char* fmt, ...) G_GNUC_PRINTF(1, 2);

#define g_warning(...) g_shim_log("WARNING", __VA_ARGS__)
#define g_warning_once(...) g_shim_log("WARNING", __VA_ARGS__)
#define g_critical(...) g_shim_log("CRITICAL", __VA_ARGS__)
#define g_message(...) g_shim_log("MESSAGE", __VA_ARGS__)
#define g_debug(...) g_shim_log("DEBUG", __VA_ARGS__)
#define g_error(...) g_shim_fatal(__VA_ARGS__)

#define g_assert(expr)                                                      \
    do {                                                                    \
        if (!(expr))                                                        \
            g_shim_fatal("assertion failed: %s (%s:%d)", #expr, __FILE__,   \
                         __LINE__);                                         \
    } while (0)
#define g_assert_not_reached() \
    g_shim_fatal("code should not be reached (%s:%d)", __FILE__, __LINE__)
#define g_return_if_fail(expr)                                              \
    do {                                                                    \
        if (!(expr)) {                                                      \
            g_shim_log("CRITICAL", "%s: assertion '%s' failed", G_STRFUNC,  \
                       #expr);                                              \
            return;                                                         \
        }                                                                   \
    } while (0)
#define g_return_val_if_fail(expr, val)                                     \
    do {                                                                    \
        if (!(expr)) {                                                      \
            g_shim_log("CRITICAL", "%s: assertion '%s' failed", G_STRFUNC,  \
                       #expr);                                              \
            return (val);                                                   \
        }                                                                   \
    } while (0)
#define g_warn_if_fail(expr)                                               \
    do {                                                                    \
        if (!(expr))                                                        \
            g_shim_log("WARNING", "%s: condition '%s' failed", G_STRFUNC,   \
                       #expr);                                              \
    } while (0)
#define g_warn_if_reached() \
    g_shim_log("WARNING", "%s: code should not be reached", G_STRFUNC)

void* g_malloc(size_t n);
void* g_malloc0(size_t n);
void* g_realloc(void* p, size_t n);
void g_free(void* p);
char* g_strdup(const char* s);
#define g_new(type, n) ((type*)g_malloc(sizeof(type) * (size_t)(n)))
#define g_new0(type, n) ((type*)g_malloc0(sizeof(type) * (size_t)(n)))

gsize g_strlcpy(char* dst, const char* src, gsize size);
gint g_snprintf(char* buf, gsize n, const char* fmt, ...) G_GNUC_PRINTF(3, 4);
gint g_vsnprintf(char* buf, gsize n, const char* fmt, va_list ap);
gboolean g_str_has_prefix(const char* s, const char* prefix);
char* g_strstr_len(const char* haystack, gssize len, const char* needle);
gint g_ascii_strcasecmp(const char* s1, const char* s2);
const char* g_strerror(int err);
const char* g_getenv(const char* name);
guint g_strv_length(char** strv);
void g_strfreev(char** strv);

typedef struct _GString {
    gchar* str;
    gsize len;
    gsize allocated_len;
} GString;
GString* g_string_new(const char* init);
void g_string_append_printf(GString* s, const char* fmt, ...) G_GNUC_PRINTF(2, 3);
char* g_string_free(GString* s, gboolean free_segment);

typedef struct _GRand GRand;
GRand* g_rand_new(void);
void g_rand_free(GRand* r);
gint32 g_rand_int_range(GRand* r, gint32 begin, gint32 end);

typedef struct _GError {
    guint32 domain;
    gint code;
    gchar* message;
} GError;
void g_error_free(GError* e);

typedef struct _GDebugKey {
    const gchar* key;
    guint value;
} GDebugKey;
guint g_parse_debug_string(const gchar* str, const GDebugKey* keys, guint nkeys);

/* Exec/guestfwd forwarding is unsupported; these stubs always fail. */
typedef void* GPid;
typedef void (*GSpawnChildSetupFunc)(gpointer user_data);
typedef enum { G_SPAWN_DEFAULT = 0, G_SPAWN_SEARCH_PATH = 1 << 3 } GSpawnFlags;
gboolean g_shell_parse_argv(const gchar* cmd, gint* argcp, gchar*** argvp, GError** error);
gboolean g_spawn_async(const gchar* wd, gchar** argv, gchar** envp, GSpawnFlags flags,
                       GSpawnChildSetupFunc child_setup, gpointer user_data,
                       GPid* child_pid, GError** error);
gboolean g_spawn_async_with_fds(const gchar* wd, gchar** argv, gchar** envp, GSpawnFlags flags,
                                GSpawnChildSetupFunc child_setup, gpointer user_data,
                                GPid* child_pid, gint stdin_fd, gint stdout_fd,
                                gint stderr_fd, GError** error);

#ifdef __cplusplus
}
#endif

#endif
