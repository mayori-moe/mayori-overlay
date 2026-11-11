/*
 * Minimal glibc ABI compatibility shim for musl.
 *
 * musl's loader deliberately redirects the sonames libc.so.6, libm.so.6,
 * libpthread.so.0, librt.so.1 and libdl.so.2 to itself, so a shim named
 * libc.so.6 would never be opened.  Instead this library keeps a
 * non-reserved name and is injected with LD_PRELOAD; its symbols then
 * live in the global scope.  musl already exports the vast majority of
 * the libc ABI used by the NVIDIA user-space binaries, so only the
 * glibc-only entry points, the LFS aliases and the _FORTIFY wrappers are
 * provided here.  musl ignores GLIBC_2.x version tags and resolves by
 * symbol name.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/sendfile.h>
#include <sys/syslog.h>
#include <sys/random.h>
#include <stdint.h>
#include <time.h>
#include <locale.h>

/* musl provides these as macros when _GNU_SOURCE is set */
#undef readdir64
#undef readdir64_r
#undef lseek64
#undef fseeko64
#undef ftello64
#undef ftruncate64
#undef truncate64
#undef fallocate64
#undef posix_fadvise64
#undef sendfile64
#undef statfs64
#undef fstatfs64
#undef statvfs64
#undef fstatvfs64

/* ------------------------------------------------------------------ */
/* glibc mutex ABI fixup                                              */
/* ------------------------------------------------------------------ */

/*
 * glibc and musl use different pthread_mutex_t layouts:
 *
 *   glibc x86_64: __lock@0 __count@4 __owner@8 __nusers@12 __kind@16 ...
 *   musl  x86_64: __type@0 __lock@4 __waiters@8 ...           __count@20
 *
 * A C++ object compiled against glibc's libstdc++ may statically
 * initialise a recursive/error-check mutex with glibc's
 * PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, which only sets a 1 at
 * byte offset 16.  musl then sees a normal, unlocked mutex and a
 * recursive lock deadlocks.  Detect that exact signature on first use
 * and translate it to the musl layout.
 */
typedef int (*pthread_mutex_fn)(pthread_mutex_t *);
typedef int (*pthread_mutex_timed_fn)(pthread_mutex_t *,
                                      const struct timespec *);

static pthread_mutex_fn real_mutex_lock;
static pthread_mutex_fn real_mutex_trylock;
static pthread_mutex_timed_fn real_mutex_timedlock;
static int pthread_resolving;

static void resolve_pthread_mutex(void)
{
	if (pthread_resolving)
		return;
	pthread_resolving = 1;
	if (!real_mutex_lock)
		real_mutex_lock = (pthread_mutex_fn)
			dlsym(RTLD_NEXT, "pthread_mutex_lock");
	if (!real_mutex_trylock)
		real_mutex_trylock = (pthread_mutex_fn)
			dlsym(RTLD_NEXT, "pthread_mutex_trylock");
	if (!real_mutex_timedlock)
		real_mutex_timedlock = (pthread_mutex_timed_fn)
			dlsym(RTLD_NEXT, "pthread_mutex_timedlock");
	pthread_resolving = 0;
}

static void fix_glibc_mutex(pthread_mutex_t *m)
{
	int *p = (int *)m;
	int i, kind, allzero = 1;

	/* Only an untouched glibc static initializer has this shape. */
	for (i = 0; i < 10; i++)
		if (i != 4 && p[i] != 0)
			allzero = 0;
	kind = p[4];
	if (!allzero || kind < 0 || kind > 3)
		return;
	if (kind == 0)
		return;

	/* musl PTHREAD_MUTEX_RECURSIVE = 1, ERRORCHECK = 2. */
	p[0] = (kind == 3) ? 0 : kind;
	p[4] = 0;
}

static int shim_mutex_lock(pthread_mutex_t *m)
{
	fix_glibc_mutex(m);
	resolve_pthread_mutex();
	return real_mutex_lock ? real_mutex_lock(m) : 0;
}

static int shim_mutex_trylock(pthread_mutex_t *m)
{
	fix_glibc_mutex(m);
	resolve_pthread_mutex();
	return real_mutex_trylock ? real_mutex_trylock(m) : 0;
}

static int shim_mutex_timedlock(pthread_mutex_t *m, const struct timespec *ts)
{
	fix_glibc_mutex(m);
	resolve_pthread_mutex();
	return real_mutex_timedlock ? real_mutex_timedlock(m, ts) : 0;
}

int pthread_mutex_lock(pthread_mutex_t *m)
{
	return shim_mutex_lock(m);
}

int pthread_mutex_trylock(pthread_mutex_t *m)
{
	return shim_mutex_trylock(m);
}

int pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *ts)
{
	return shim_mutex_timedlock(m, ts);
}

/*
 * Same-length private names.  A DT_NEEDED shim cannot interpose symbols
 * that musl already exports (musl's libc is added to the global symbol
 * table before any dlopen'ed dependencies), so the prebuilt ELFs are
 * rewritten to call these names instead.  They are absent from musl and
 * therefore always resolve to the shim.
 */
int nvidia_mutex_lock_(pthread_mutex_t *m)
{
	return shim_mutex_lock(m);
}

int nvidia_mutex_trylock_(pthread_mutex_t *m)
{
	return shim_mutex_trylock(m);
}

int nvidia_mutex_timedlock_(pthread_mutex_t *m, const struct timespec *ts)
{
	return shim_mutex_timedlock(m, ts);
}

/* ------------------------------------------------------------------ */
/* glibc internal aliases                                             */
/* ------------------------------------------------------------------ */

#undef __isnan
#undef __isnanf
#undef __isnand
#undef __isinf
#undef __isinff
#undef __isinfd
int __isnan(double x)  { return __builtin_isnan(x); }
int __isnanf(float x)  { return __builtin_isnan(x); }
int __isnand(double x) { return __builtin_isnan(x); }
int __isinf(double x)  { return __builtin_isinf(x); }
int __isinff(float x)  { return __builtin_isinf(x); }
int __isinfd(double x) { return __builtin_isinf(x); }

void __longjmp_chk(jmp_buf env, int val)
{
	longjmp(env, val);
}

#undef __strdup
char *__strdup(const char *s) { return strdup(s); }

#undef __strtok_r
char *__strtok_r(char *s, const char *delim, char **saveptr)
{
	return strtok_r(s, delim, saveptr);
}

/*
 * glibc's CPU_SET macros can allocate a dynamically sized cpu_set_t.
 * musl's cpu_set_t covers 1024 CPUs; keep at least that much so that a
 * glibc caller using CPU_ALLOC()/CPU_FREE() is safe.
 */
void *__sched_cpualloc(size_t count)
{
	size_t bytes = (count + 7) / 8;
	if (bytes < 128)
		bytes = 128;
	return calloc(1, bytes);
}

void __sched_cpufree(void *set) { free(set); }

/* Conservative: never advertise a single-threaded process to libraries
 * that use this glibc 2.32+ fast-path flag. */
char __libc_single_threaded = 0;

/* ------------------------------------------------------------------ */
/* dl* extensions                                                     */
/* ------------------------------------------------------------------ */

void *dlvsym(void *handle, const char *symbol, const char *version)
{
	(void)version;
	return dlsym(handle, symbol);
}

void *dlmopen(long nsid, const char *file, int mode)
{
	(void)nsid;
	return dlopen(file, mode);
}

int __register_atfork(void (*prepare)(void), void (*parent)(void),
                      void (*child)(void), void *dso_handle)
{
	(void)dso_handle;
	return pthread_atfork(prepare, parent, child);
}

/* ------------------------------------------------------------------ */
/* LFS aliases (x86_64: 64-bit off_t/stat are the native ones)        */
/* ------------------------------------------------------------------ */

#undef stat64
#undef fstat64
#undef lstat64
#undef fstatat64
#undef open64
#undef openat64
#undef fopen64
#undef freopen64
#undef mmap64
#undef pread64
#undef pwrite64
#undef scandir64
#undef alphasort64
#undef dirent64
#undef __xstat64
#undef __fxstat64
#undef __lxstat64
#undef __fxstatat64

int stat64(const char *path, struct stat *buf) { return stat(path, buf); }
int fstat64(int fd, struct stat *buf) { return fstat(fd, buf); }
int lstat64(const char *path, struct stat *buf) { return lstat(path, buf); }
int fstatat64(int fd, const char *path, struct stat *buf, int flags)
{
	return fstatat(fd, path, buf, flags);
}

int __xstat64(int ver, const char *path, struct stat *buf)
{
	(void)ver;
	return stat(path, buf);
}
int __fxstat64(int ver, int fd, struct stat *buf)
{
	(void)ver;
	return fstat(fd, buf);
}
int __lxstat64(int ver, const char *path, struct stat *buf)
{
	(void)ver;
	return lstat(path, buf);
}
int __fxstatat64(int ver, int fd, const char *path, struct stat *buf, int flags)
{
	(void)ver;
	return fstatat(fd, path, buf, flags);
}

int open64(const char *path, int flags, ...)
{
	mode_t mode = 0;
	va_list ap;

	if (flags & O_CREAT) {
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	return open(path, flags, mode);
}

int openat64(int dirfd, const char *path, int flags, ...)
{
	mode_t mode = 0;
	va_list ap;

	if (flags & O_CREAT) {
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	return openat(dirfd, path, flags, mode);
}

FILE *fopen64(const char *path, const char *mode) { return fopen(path, mode); }
FILE *freopen64(const char *path, const char *mode, FILE *f)
{
	return freopen(path, mode, f);
}

void *mmap64(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	return mmap(addr, len, prot, flags, fd, off);
}

ssize_t pread64(int fd, void *buf, size_t count, off_t off)
{
	return pread(fd, buf, count, off);
}
ssize_t pwrite64(int fd, const void *buf, size_t count, off_t off)
{
	return pwrite(fd, buf, count, off);
}

int scandir64(const char *dir, struct dirent ***namelist,
              int (*sel)(const struct dirent *),
              int (*compar)(const struct dirent **, const struct dirent **))
{
	return scandir(dir, namelist, sel, compar);
}

int alphasort64(const struct dirent **a, const struct dirent **b)
{
	return alphasort(a, b);
}

/* ------------------------------------------------------------------ */
/* _FORTIFY_SOURCE wrappers                                           */
/* ------------------------------------------------------------------ */

void *__memcpy_chk(void *dst, const void *src, size_t len, size_t dstlen)
{
	(void)dstlen;
	return memcpy(dst, src, len);
}
void *__memmove_chk(void *dst, const void *src, size_t len, size_t dstlen)
{
	(void)dstlen;
	return memmove(dst, src, len);
}
void *__memset_chk(void *dst, int c, size_t len, size_t dstlen)
{
	(void)dstlen;
	return memset(dst, c, len);
}
void *__mempcpy_chk(void *dst, const void *src, size_t len, size_t dstlen)
{
	(void)dstlen;
	return mempcpy(dst, src, len);
}
char *__stpcpy_chk(char *dst, const char *src, size_t dstlen)
{
	(void)dstlen;
	return stpcpy(dst, src);
}
char *__stpncpy_chk(char *dst, const char *src, size_t len, size_t dstlen)
{
	(void)dstlen;
	return stpncpy(dst, src, len);
}
char *__strcpy_chk(char *dst, const char *src, size_t dstlen)
{
	(void)dstlen;
	return strcpy(dst, src);
}
char *__strncpy_chk(char *dst, const char *src, size_t len, size_t dstlen)
{
	(void)dstlen;
	return strncpy(dst, src, len);
}
char *__strcat_chk(char *dst, const char *src, size_t dstlen)
{
	(void)dstlen;
	return strcat(dst, src);
}
char *__strncat_chk(char *dst, const char *src, size_t len, size_t dstlen)
{
	(void)dstlen;
	return strncat(dst, src, len);
}
size_t __strnlen_chk(const char *s, size_t maxlen, size_t slen)
{
	(void)slen;
	return strnlen(s, maxlen);
}
char *__strchr_chk(const char *s, int c, size_t slen)
{
	(void)slen;
	return strchr(s, c);
}
char *__strrchr_chk(const char *s, int c, size_t slen)
{
	(void)slen;
	return strrchr(s, c);
}

/* glibc 2.36+ arc4random family, used by libstdc++'s hash seeds. */
unsigned int arc4random(void)
{
	unsigned int r;
	size_t done = 0;
	while (done < sizeof r) {
		ssize_t n = getrandom((char *)&r + done, sizeof r - done, 0);
		if (n <= 0) {
			static unsigned int seed;
			seed = seed * 1664525u + 1013904223u;
			return seed ^ (unsigned int)time(NULL)
			       ^ (unsigned int)getpid();
		}
		done += (size_t)n;
	}
	return r;
}

void arc4random_buf(void *buf, size_t n)
{
	unsigned char *p = buf;
	while (n) {
		ssize_t r = getrandom(p, n, 0);
		if (r <= 0) {
			unsigned int v = arc4random();
			size_t k = n < sizeof v ? n : sizeof v;
			memcpy(p, &v, k);
			r = (ssize_t)k;
		}
		p += r;
		n -= (size_t)r;
	}
}

unsigned int arc4random_uniform(unsigned int upper_bound)
{
	unsigned int r, min;

	if (upper_bound < 2)
		return 0;
	min = -upper_bound % upper_bound;
	for (;;) {
		r = arc4random();
		if (r >= min)
			return r % upper_bound;
	}
}

/* C23 strto* entry points, referenced by newer libstdc++. */
long __isoc23_strtol(const char *nptr, char **endptr, int base)
{
	return strtol(nptr, endptr, base);
}
unsigned long __isoc23_strtoul(const char *nptr, char **endptr, int base)
{
	return strtoul(nptr, endptr, base);
}
long long __isoc23_strtoll(const char *nptr, char **endptr, int base)
{
	return strtoll(nptr, endptr, base);
}
unsigned long long __isoc23_strtoull(const char *nptr, char **endptr, int base)
{
	return strtoull(nptr, endptr, base);
}

/*
 * Compiler-runtime ABI used by the legacy (libunwind based) variant.
 * These let the private libstdc++ avoid libgcc; _Unwind_* comes from
 * libunwind.so.1.
 */
int __popcountdi2(unsigned long a)
{
	return __builtin_popcountl(a);
}

typedef unsigned __int128 u128;

static u128 udiv128(u128 n, u128 d)
{
	u128 q = 0;
	int shift = 0;

	if (!d)
		return 0;
	while ((d << 1) > d && (d << 1) <= n) {
		d <<= 1;
		shift++;
	}
	for (;;) {
		if (n >= d) {
			n -= d;
			q |= (u128)1 << shift;
		}
		if (!shift)
			break;
		d >>= 1;
		shift--;
	}
	return q;
}

__int128 __udivti3(u128 n, u128 d)
{
	return (__int128)udiv128(n, d);
}

__int128 __udivmodti4(u128 n, u128 d, u128 *rem)
{
	u128 q = udiv128(n, d);
	if (rem)
		*rem = n - q * d;
	return (__int128)q;
}

int __syslog_chk(int pri, int flag, const char *fmt, ...)
{
	va_list ap;
	(void)flag;
	va_start(ap, fmt);
	vsyslog(pri, fmt, ap);
	va_end(ap);
	return 0;
}

void __chk_fail(void) { abort(); }
void __fortify_fail(const char *msg) { (void)msg; abort(); }
void *__rawmemchr(const void *s, int c)
{
	const unsigned char *p = s;
	while (*p != (unsigned char)c)
		p++;
	return (void *)p;
}

struct cmsghdr *__cmsg_nxthdr(struct msghdr *msg, struct cmsghdr *cmsg)
{
	unsigned char *next;

	if (cmsg->cmsg_len == 0)
		return NULL;
	next = (unsigned char *)cmsg + CMSG_ALIGN(cmsg->cmsg_len);
	if (next + sizeof(struct cmsghdr) >
	    (unsigned char *)msg->msg_control + msg->msg_controllen)
		return NULL;
	return (struct cmsghdr *)next;
}

int __printf_chk(int flag, const char *fmt, ...)
{
	va_list ap;
	int r;
	(void)flag;
	va_start(ap, fmt);
	r = vprintf(fmt, ap);
	va_end(ap);
	return r;
}

int __fprintf_chk(FILE *f, int flag, const char *fmt, ...)
{
	va_list ap;
	int r;
	(void)flag;
	va_start(ap, fmt);
	r = vfprintf(f, fmt, ap);
	va_end(ap);
	return r;
}

int __vprintf_chk(int flag, const char *fmt, va_list ap)
{
	(void)flag;
	return vprintf(fmt, ap);
}

int __vfprintf_chk(FILE *f, int flag, const char *fmt, va_list ap)
{
	(void)flag;
	return vfprintf(f, fmt, ap);
}

int __sprintf_chk(char *dst, int flag, size_t dstlen, const char *fmt, ...)
{
	va_list ap;
	int r;
	(void)flag;
	(void)dstlen;
	va_start(ap, fmt);
	r = vsprintf(dst, fmt, ap);
	va_end(ap);
	return r;
}

int __snprintf_chk(char *dst, size_t maxlen, int flag, size_t dstlen,
                   const char *fmt, ...)
{
	va_list ap;
	int r;
	(void)flag;
	(void)dstlen;
	va_start(ap, fmt);
	r = vsnprintf(dst, maxlen, fmt, ap);
	va_end(ap);
	return r;
}

int __vsprintf_chk(char *dst, int flag, size_t dstlen, const char *fmt,
                   va_list ap)
{
	(void)flag;
	(void)dstlen;
	return vsprintf(dst, fmt, ap);
}

int __vsnprintf_chk(char *dst, size_t maxlen, int flag, size_t dstlen,
                    const char *fmt, va_list ap)
{
	(void)flag;
	(void)dstlen;
	return vsnprintf(dst, maxlen, fmt, ap);
}

char *__realpath_chk(const char *path, char *resolved, size_t resolvedlen)
{
	(void)resolvedlen;
	return realpath(path, resolved);
}

int __open_2(const char *path, int flags)
{
	return open(path, flags);
}

int __openat_2(int dirfd, const char *path, int flags)
{
	return openat(dirfd, path, flags);
}

ssize_t __read_chk(int fd, void *buf, size_t count, size_t buflen)
{
	if (count > buflen) {
		errno = EINVAL;
		return -1;
	}
	return read(fd, buf, count);
}

ssize_t __recv_chk(int fd, void *buf, size_t len, size_t buflen, int flags)
{
	if (len > buflen) {
		errno = EINVAL;
		return -1;
	}
	return recv(fd, buf, len, flags);
}

ssize_t __recvfrom_chk(int fd, void *buf, size_t len, size_t buflen,
                       int flags, struct sockaddr *addr, socklen_t *addrlen)
{
	if (len > buflen) {
		errno = EINVAL;
		return -1;
	}
	return recvfrom(fd, buf, len, flags, addr, addrlen);
}

long __fdelt_chk(long fd)
{
	if (fd < 0 || fd >= FD_SETSIZE) {
		fprintf(stderr, "__fdelt_chk: file descriptor out of range\n");
		abort();
	}
	return fd / (8 * sizeof(long));
}

int __poll_chk(struct pollfd *fds, nfds_t nfds, int timeout, size_t fdslen)
{
	if (nfds * sizeof(struct pollfd) > fdslen) {
		errno = EINVAL;
		return -1;
	}
	return poll(fds, nfds, timeout);
}

int __select_chk(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                 struct timeval *timeout, size_t fdslen)
{
	(void)fdslen;
	return select(nfds, readfds, writefds, exceptfds, timeout);
}

/* ------------------------------------------------------------------ */
/* remaining glibc-only entry points                                  */
/* ------------------------------------------------------------------ */

int __pthread_key_create(pthread_key_t *key, void (*destr)(void *))
{
	return pthread_key_create(key, destr);
}

/*
 * Thread-local C++ destructors.  musl does not expose its internal TLS
 * dtor list, so keep a per-thread list attached to a pthread key; the
 * key destructor runs at thread exit and invokes everything registered
 * on that thread.
 */
struct thrd_dtor {
	void (*dtor)(void *);
	void *obj;
	struct thrd_dtor *next;
};

static pthread_key_t thrd_dtor_key;
static pthread_once_t thrd_dtor_once = PTHREAD_ONCE_INIT;

static void run_thrd_dtors(void *p)
{
	struct thrd_dtor *e, *next;

	for (e = p; e; e = next) {
		next = e->next;
		e->dtor(e->obj);
		free(e);
	}
}

static void make_thrd_dtor_key(void)
{
	pthread_key_create(&thrd_dtor_key, run_thrd_dtors);
}

int __cxa_thread_atexit_impl(void (*dtor)(void *), void *obj, void *dso_symbol)
{
	struct thrd_dtor *e;
	(void)dso_symbol;

	pthread_once(&thrd_dtor_once, make_thrd_dtor_key);
	e = malloc(sizeof *e);
	if (!e)
		return -1;
	e->dtor = dtor;
	e->obj = obj;
	e->next = pthread_getspecific(thrd_dtor_key);
	pthread_setspecific(thrd_dtor_key, e);
	return 0;
}

const char *gnu_get_libc_version(void)
{
	return "2.34";
}

const char *__secure_getenv(const char *name)
{
	return secure_getenv(name);
}

/* glibc's struct mallinfo ABI: ten ints, all zero is always safe. */
struct mallinfo {
	int arena;
	int ordblks;
	int smblks;
	int hblks;
	int hblkhd;
	int usmblks;
	int fsmblks;
	int uordblks;
	int fordblks;
	int keepcost;
};

struct mallinfo mallinfo(void)
{
	struct mallinfo mi = {0};
	return mi;
}

int backtrace(void **buffer, int size)
{
	(void)buffer;
	(void)size;
	return 0;
}

char **backtrace_symbols(void *const *buffer, int size)
{
	(void)buffer;
	(void)size;
	return NULL;
}

void backtrace_symbols_fd(void *const *buffer, int size, int fd)
{
	(void)buffer;
	(void)size;
	(void)fd;
}

int dladdr1(const void *addr, Dl_info *info, void **extra_info, int flags)
{
	(void)flags;
	if (extra_info)
		*extra_info = NULL;
	return dladdr(addr, info);
}

size_t __strftime_l(char *s, size_t max, const char *fmt,
                    const struct tm *tm, locale_t loc)
{
	(void)loc;
	return strftime(s, max, fmt, tm);
}

/* RPC entry points formerly in glibc; only used by nvidia-persistenced. */
int xdr_int(void *xdrs, int *ip)
{
	(void)xdrs;
	(void)ip;
	errno = ENOSYS;
	return 0;
}

int xdr_enum(void *xdrs, int *ep)
{
	return xdr_int(xdrs, ep);
}

void *clntunix_create(void *addr, unsigned long *len, int *fd, unsigned int sendsz,
                      unsigned int recvsz)
{
	(void)addr;
	(void)len;
	(void)fd;
	(void)sendsz;
	(void)recvsz;
	errno = ENOSYS;
	return NULL;
}

/* ------------------------------------------------------------------ */
/* LFS, part 2                                                        */
/* ------------------------------------------------------------------ */

int readdir64_r(DIR *dir, struct dirent *entry, struct dirent **result)
{
	struct dirent *d = readdir(dir);
	if (!d) {
		*result = NULL;
		return 0;
	}
	*entry = *d;
	*result = entry;
	return 0;
}

struct dirent *readdir64(DIR *dir)
{
	return readdir(dir);
}

off_t lseek64(int fd, off_t off, int whence) { return lseek(fd, off, whence); }
int fseeko64(FILE *f, off_t off, int whence) { return fseeko(f, off, whence); }
off_t ftello64(FILE *f) { return ftello(f); }
int ftruncate64(int fd, off_t len) { return ftruncate(fd, len); }
int truncate64(const char *path, off_t len) { return truncate(path, len); }
int fallocate64(int fd, int mode, off_t off, off_t len)
{
	return fallocate(fd, mode, off, len);
}
int posix_fadvise64(int fd, off_t off, off_t len, int advice)
{
	return posix_fadvise(fd, off, len, advice);
}
ssize_t sendfile64(int out_fd, int in_fd, off_t *off, size_t count)
{
	return sendfile(out_fd, in_fd, off, count);
}
int statfs64(const char *path, struct statfs *buf) { return statfs(path, buf); }
int fstatfs64(int fd, struct statfs *buf) { return fstatfs(fd, buf); }
int statvfs64(const char *path, struct statvfs *buf) { return statvfs(path, buf); }
int fstatvfs64(int fd, struct statvfs *buf) { return fstatvfs(fd, buf); }

/*
 * IEEE binary128 formatting/conversion.  NVIDIA's GPU compiler and
 * libstdc++ reference these for __float128; there is no quadmath on a
 * musl base system, so the formatting emits a harmless placeholder and
 * the parser returns zero rather than failing to link.
 */
int strfromf128(char *s, size_t n, const char *format, long double fp)
{
	(void)fp;
	(void)format;
	if (n)
		s[0] = '0';
	if (n > 1)
		s[1] = 0;
	return 1;
}

__float128 strtof128(const char *nptr, char **endptr)
{
	(void)nptr;
	if (endptr)
		*endptr = (char *)nptr;
	return 0;
}
