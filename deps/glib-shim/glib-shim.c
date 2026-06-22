/* Minimal glib shim implementation for libslirp. See glib.h. */
#include "glib.h"

#include <time.h>

void g_shim_log(const char* level, const char* fmt, ...) {
    va_list ap;

    fprintf(stderr, "slirp-%s: ", level);

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
}

void g_shim_fatal(const char* fmt, ...) {
    va_list ap;

    fprintf(stderr, "slirp-ERROR: ");

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);

    abort();
}

void* g_malloc(size_t n) {
    if (!n)
        return NULL;

    void* p = malloc(n);

    if (!p)
        g_shim_fatal("g_malloc(%zu) failed", n);

    return p;
}

void* g_malloc0(size_t n) {
    if (!n)
        return NULL;

    void* p = calloc(1, n);

    if (!p)
        g_shim_fatal("g_malloc0(%zu) failed", n);

    return p;
}

void* g_realloc(void* p, size_t n) {
    if (!n) {
        free(p);

        return NULL;
    }

    void* np = realloc(p, n);

    if (!np)
        g_shim_fatal("g_realloc(%zu) failed", n);

    return np;
}

void g_free(void* p) {
    free(p);
}

char* g_strdup(const char* s) {
    if (!s)
        return NULL;

    size_t n = strlen(s) + 1;
    char* p = (char*)g_malloc(n);

    memcpy(p, s, n);

    return p;
}

gsize g_strlcpy(char* dst, const char* src, gsize size) {
    gsize srclen = strlen(src);

    if (size) {
        gsize n = (srclen < size - 1) ? srclen : size - 1;

        memcpy(dst, src, n);
        dst[n] = '\0';
    }

    return srclen;
}

gint g_vsnprintf(char* buf, gsize n, const char* fmt, va_list ap) {
    return vsnprintf(buf, n, fmt, ap);
}

gint g_snprintf(char* buf, gsize n, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    gint r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);

    return r;
}

gboolean g_str_has_prefix(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

char* g_strstr_len(const char* haystack, gssize len, const char* needle) {
    if (len < 0)
        return (char*)strstr(haystack, needle);

    size_t nl = strlen(needle);

    if (nl == 0)
        return (char*)haystack;

    for (gssize i = 0; i + (gssize)nl <= len; i++) {
        if (strncmp(haystack + i, needle, nl) == 0)
            return (char*)(haystack + i);
    }

    return NULL;
}

gint g_ascii_strcasecmp(const char* s1, const char* s2) {
    for (;; s1++, s2++) {
        int c1 = (unsigned char)*s1;
        int c2 = (unsigned char)*s2;

        if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';

        if (c1 != c2 || c1 == 0)
            return c1 - c2;
    }
}

const char* g_strerror(int err) {
    return strerror(err);
}

const char* g_getenv(const char* name) {
    return getenv(name);
}

guint g_strv_length(char** strv) {
    guint n = 0;

    if (strv)
        while (strv[n])
            n++;

    return n;
}

void g_strfreev(char** strv) {
    if (!strv)
        return;

    for (char** p = strv; *p; p++)
        g_free(*p);

    g_free(strv);
}

GString* g_string_new(const char* init) {
    GString* s = (GString*)g_malloc(sizeof(GString));

    s->len = init ? strlen(init) : 0;
    s->allocated_len = s->len + 16;
    s->str = (gchar*)g_malloc(s->allocated_len);

    if (init)
        memcpy(s->str, init, s->len);

    s->str[s->len] = '\0';

    return s;
}

static void g_string_grow(GString* s, gsize extra) {
    gsize need = s->len + extra + 1;

    if (need <= s->allocated_len)
        return;

    while (s->allocated_len < need)
        s->allocated_len *= 2;

    s->str = (gchar*)g_realloc(s->str, s->allocated_len);
}

void g_string_append_printf(GString* s, const char* fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0)
        return;

    g_string_grow(s, (gsize)n);

    va_start(ap, fmt);
    vsnprintf(s->str + s->len, (gsize)n + 1, fmt, ap);
    va_end(ap);

    s->len += (gsize)n;
}

char* g_string_free(GString* s, gboolean free_segment) {
    if (!s)
        return NULL;

    char* ret = NULL;

    if (free_segment)
        g_free(s->str);
    else
        ret = s->str;

    g_free(s);

    return ret;
}

struct _GRand {
    uint64_t state;
};

static uint64_t g_rand_seed_counter = 0x9e3779b97f4a7c15ull;

GRand* g_rand_new(void) {
    GRand* r = (GRand*)g_malloc(sizeof(GRand));

    g_rand_seed_counter += 0x9e3779b97f4a7c15ull;
    r->state = g_rand_seed_counter ^ ((uint64_t)time(NULL) << 16) ^ (uintptr_t)r;

    if (!r->state)
        r->state = 0x123456789abcdefull;

    return r;
}

void g_rand_free(GRand* r) {
    g_free(r);
}

static uint32_t g_rand_next(GRand* r) {
    /* xorshift64* */
    uint64_t x = r->state;

    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;

    r->state = x;

    return (uint32_t)((x * 0x2545f4914f6cdd1dull) >> 32);
}

gint32 g_rand_int_range(GRand* r, gint32 begin, gint32 end) {
    if (end <= begin)
        return begin;

    uint32_t span = (uint32_t)(end - begin);

    return begin + (gint32)(g_rand_next(r) % span);
}

void g_error_free(GError* e) {
    if (!e)
        return;

    g_free(e->message);
    g_free(e);
}

static void g_shim_set_error(GError** error, const char* message) {
    if (!error)
        return;

    GError* e = (GError*)g_malloc0(sizeof(GError));

    e->message = g_strdup(message);
    *error = e;
}

guint g_parse_debug_string(const gchar* str, const GDebugKey* keys, guint nkeys) {
    if (!str)
        return 0;

    if (strcmp(str, "all") == 0) {
        guint mask = 0;

        for (guint i = 0; i < nkeys; i++)
            mask |= keys[i].value;

        return mask;
    }

    guint result = 0;
    const char* p = str;

    while (*p) {
        while (*p == ' ' || *p == ',' || *p == ':' || *p == ';' || *p == '\t')
            p++;

        const char* start = p;

        while (*p && *p != ' ' && *p != ',' && *p != ':' && *p != ';' && *p != '\t')
            p++;

        size_t len = (size_t)(p - start);

        for (guint i = 0; i < nkeys; i++) {
            if (strlen(keys[i].key) == len && strncmp(start, keys[i].key, len) == 0)
                result |= keys[i].value;
        }
    }

    return result;
}

gboolean g_shell_parse_argv(const gchar* cmd, gint* argcp, gchar*** argvp, GError** error) {
    (void)cmd;

    if (argcp)
        *argcp = 0;
    if (argvp)
        *argvp = NULL;

    g_shim_set_error(error, "exec forwarding is not supported");

    return FALSE;
}

gboolean g_spawn_async(const gchar* wd, gchar** argv, gchar** envp, GSpawnFlags flags,
                       GSpawnChildSetupFunc child_setup, gpointer user_data,
                       GPid* child_pid, GError** error) {
    (void)wd; (void)argv; (void)envp; (void)flags;
    (void)child_setup; (void)user_data; (void)child_pid;

    g_shim_set_error(error, "exec forwarding is not supported");

    return FALSE;
}

gboolean g_spawn_async_with_fds(const gchar* wd, gchar** argv, gchar** envp, GSpawnFlags flags,
                                GSpawnChildSetupFunc child_setup, gpointer user_data,
                                GPid* child_pid, gint stdin_fd, gint stdout_fd,
                                gint stderr_fd, GError** error) {
    (void)wd; (void)argv; (void)envp; (void)flags;
    (void)child_setup; (void)user_data; (void)child_pid;
    (void)stdin_fd; (void)stdout_fd; (void)stderr_fd;

    g_shim_set_error(error, "exec forwarding is not supported");

    return FALSE;
}
