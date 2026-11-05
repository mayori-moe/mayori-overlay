/*
 * mimalloc as musl's malloc implementation.
 *
 * This file is part of musl's "external" allocator directory, selected with
 *     ./configure --with-malloc=external
 *
 * It is a *new* integration file: neither the musl sources nor the mimalloc
 * sources are modified.  mimalloc is built as a single translation unit
 * (its upstream src/static.c) and this file only provides the entry points
 * expected by musl's libc internals:
 *
 *     __libc_malloc_impl, __libc_calloc, __libc_free, __libc_realloc,
 *     calloc, aligned_alloc, malloc_usable_size, __malloc_donate
 *
 * The Makefile places the mimalloc source tree next to the musl source tree
 * (../mimalloc) and adds the needed include flags for this translation unit.
 *
 * Early bootstrap allocations
 * ---------------------------
 * musl's dynamic loader (ldso) allocates a few objects before the final
 * thread-local storage block is installed with __copy_tls().  mimalloc uses
 * thread-locals/pthread keys internally, and the loader's temporary pthread
 * structure is discarded at that point.  To avoid initializing mimalloc
 * against temporary TLS state, allocations made before the libc init-array
 * (and therefore before mimalloc's process constructor) are served by a
 * small bump allocator.  Those allocations are never reclaimed; they are
 * ordinary startup allocations (loader data, initial TLS block, ...).
 *
 * Once the constructor has run, all allocation functions go to mimalloc.
 */

/* ---- mimalloc configuration ------------------------------------------ */

/* We are built as part of the C library itself. */
#define MI_LIBC_MUSL 1

/* Do not inspect MIMALLOC_* environment variables from within libc. */
#define MI_NO_GETENV 1

/* We provide the process attach/detach hooks ourselves below. */
#define MI_PRIM_HAS_PROCESS_ATTACH 1

/* mimalloc's default TLS model uses __thread variables.  musl's libc.so is
 * also the dynamic loader and is not registered as a TLS module for its own
 * use, so no __thread variables may be used here.  The pthread-key model
 * needs no TLS data and works both in ldso and in normal libc use. */
#define MI_TLS_MODEL_PTHREADS 1

/* mimalloc's padding is not needed here and has a measurable cost. */
#define MI_PADDING 0

/* Optional hardening for a system libc, at some performance cost:
 * #define MI_SECURE 4
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

/* musl's own thread-pointer accessor.  Using it directly avoids mimalloc's
 * fallback of a __thread variable on architectures without an inline asm
 * thread-pointer implementation (musl's libc.so cannot use TLS variables). */
#include "pthread_impl.h"

/* musl's internal <features.h> defines `weak` as a macro (it expands to
 * __attribute__((__weak__))), while mimalloc uses the token in its own
 * declarations. */
#undef weak

#define MI_PRIM_THREAD_ID() ((mi_thread_id_t)(uintptr_t)__get_tp())

/* The whole mimalloc library, built as a single object. */
#include "mimalloc/src/static.c"

/* ---- startup (pre-TLS) bump allocator -------------------------------- */

#define PRE_ALIGN  16
#define PRE_HDR    16
#define PRE_ARENAS 64

static struct {
	uintptr_t start, end;
} pre_arena[PRE_ARENAS];
static size_t pre_arena_count;
static uintptr_t pre_cur, pre_end;

static int pre_in_arena(const void *p)
{
	uintptr_t a = (uintptr_t)p;
	size_t i;
	for (i = 0; i < pre_arena_count; i++)
		if (a >= pre_arena[i].start && a < pre_arena[i].end)
			return 1;
	return 0;
}

static void *pre_more(size_t need)
{
	size_t len = need + PRE_HDR + PRE_ALIGN;
	size_t chunk = 1 << 20;
	void *m;

	if (len < need) return NULL;                    /* overflow */
	if (len < need) return NULL;			/* overflow */
	if (pre_arena_count >= PRE_ARENAS) return NULL;
	if (len > chunk) chunk = (len + 4095) & ~(size_t)4095;
	m = mmap(NULL, chunk, PROT_READ | PROT_WRITE,
	         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (m == MAP_FAILED) return NULL;
	pre_arena[pre_arena_count].start = (uintptr_t)m;
	pre_arena[pre_arena_count].end = (uintptr_t)m + chunk;
	pre_arena_count++;
	pre_cur = (uintptr_t)m;
	pre_end = pre_cur + chunk;
	return m;
}

static void *pre_alloc(size_t n, size_t align)
{
	uintptr_t p;
	size_t *hdr;

	if (align < PRE_ALIGN) align = PRE_ALIGN;
	if (align > (size_t)1 << 30 || (align & (align - 1))) return NULL;
	if (!n) n = 1;
	if (n > ((size_t)-1 >> 1)) return NULL;

	p = (pre_cur + PRE_HDR + align - 1) & ~(uintptr_t)(align - 1);
	if (!pre_cur || p + n > pre_end) {
		if (!pre_more(n + align)) return NULL;
		p = (pre_cur + PRE_HDR + align - 1) & ~(uintptr_t)(align - 1);
		if (p + n > pre_end) return NULL;
	}
	hdr = (size_t *)(p - PRE_HDR);
	hdr[0] = n;
	pre_cur = p + n;
	return (void *)p;
}

static size_t pre_size(const void *p)
{
	return *(const size_t *)((uintptr_t)p - PRE_HDR);
}

/* ---- mimalloc process lifecycle -------------------------------------- */

static volatile int mi_ready;

__attribute__((constructor(101)))
static void mi_musl_attach(void)
{
	if (!mi_ready) {
		_mi_auto_process_init();
		__atomic_store_n(&mi_ready, 1, __ATOMIC_RELEASE);
	}
}

__attribute__((destructor(101)))
static void mi_musl_detach(void)
{
	if (__atomic_load_n(&mi_ready, __ATOMIC_ACQUIRE))
		_mi_auto_process_done();
}

/* ---- musl allocator entry points ------------------------------------- */

void *__libc_malloc_impl(size_t n)
{
	void *p;

	if (__builtin_expect(!__atomic_load_n(&mi_ready, __ATOMIC_ACQUIRE), 0)) {
		p = pre_alloc(n, PRE_ALIGN);
		if (!p) errno = ENOMEM;
		return p;
	}
	p = mi_malloc(n);
	if (!p) errno = ENOMEM;
	return p;
}

void __libc_free(void *p)
{
	if (!p) return;
	if (!__atomic_load_n(&mi_ready, __ATOMIC_ACQUIRE)) return;
	if (pre_in_arena(p)) return;            /* startup allocation */
	mi_free(p);
}

void *__libc_realloc(void *p, size_t n)
{
	size_t old;
	void *q;

	if (!p) return __libc_malloc_impl(n);
	if (pre_in_arena(p)) {
		old = pre_size(p);
		if (!n) return NULL;		/* old memory is never reused */
		q = __libc_malloc_impl(n);
		if (!q) return NULL;
		memcpy(q, p, old < n ? old : n);
		return q;
	}
	q = mi_realloc(p, n);
	if (!q && n) errno = ENOMEM;
	return q;
}

static void *musl_calloc(size_t m, size_t n)
{
	size_t sz;
	void *p;

	if (n && m > (size_t)-1 / n) {
		errno = ENOMEM;
		return NULL;
	}
	sz = m * n;
	if (!__atomic_load_n(&mi_ready, __ATOMIC_ACQUIRE)) {
		p = pre_alloc(sz, PRE_ALIGN);
		if (!p) {
			errno = ENOMEM;
			return NULL;
		}
		if (sz) memset(p, 0, sz);
		return p;
	}
	p = mi_calloc(m, n);
	if (!p) errno = ENOMEM;
	return p;
}

void *__libc_calloc(size_t m, size_t n)
{
	return musl_calloc(m, n);
}

/* The public calloc() honours an application-provided malloc(), just like
 * musl's regular calloc implementation does. */
__attribute__((visibility("default")))
void *calloc(size_t m, size_t n)
{
	extern __attribute__((visibility("hidden"))) int __malloc_replaced;
	size_t sz;
	void *p;

	if (__malloc_replaced) {
		if (n && m > (size_t)-1 / n) {
			errno = ENOMEM;
			return NULL;
		}
		sz = m * n;
		p = malloc(sz);
		if (!p) return NULL;
		return memset(p, 0, sz);
	}
	return musl_calloc(m, n);
}

__attribute__((visibility("default")))
void *aligned_alloc(size_t align, size_t len)
{
	extern __attribute__((visibility("hidden"))) int __malloc_replaced;
	extern __attribute__((visibility("hidden"))) int __aligned_alloc_replaced;

	if (align && (align & (align - 1))) {
		errno = EINVAL;
		return NULL;
	}
	/* Same hardening rule as musl's mallocng: if malloc was replaced but
	 * aligned_alloc was not, don't return memory from a different heap. */
	if (__malloc_replaced && !__aligned_alloc_replaced) {
		errno = ENOMEM;
		return NULL;
	}
	if (align < PRE_ALIGN) align = PRE_ALIGN;
	if (!__atomic_load_n(&mi_ready, __ATOMIC_ACQUIRE)) {
		void *p = pre_alloc(len, align);
		if (!p) errno = ENOMEM;
		return p;
	}
	{
		void *p = mi_malloc_aligned(len, align);
		if (!p) errno = ENOMEM;
		return p;
	}
}

__attribute__((visibility("default")))
size_t malloc_usable_size(void *p)
{
	if (!p) return 0;
	if (pre_in_arena(p)) return pre_size(p);
	return mi_malloc_usable_size(p);
}

/* musl's loader donates the unused page gaps of shared objects to the
 * allocator.  mimalloc manages its own OS memory, so ignore them. */
void __malloc_donate(char *a, char *b)
{
	(void)a;
	(void)b;
}
