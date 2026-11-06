/*
 * mimalloc as musl's system allocator (optimized variant).
 */

/* ------------------------------------------------------------------------- */
/* musl integration internals                                                */
/* ------------------------------------------------------------------------- */

/* The internal musl headers use the hidden macro that is normally provided by
 * musl's src/include/features.h.  We deliberately do not use that header for
 * mimalloc (it also defines a `weak` macro which would hijack the token used
 * in mimalloc's attributes), so provide the attribute macro here. */
#ifndef hidden
#define hidden __attribute__((__visibility__("hidden")))
#endif

/* Needed for the inlined pthread TSD accessor below. */
#include "pthread_impl.h"

/* musl's internal syscall.h defines these as macros; mimalloc includes the
 * public headers, which declare the real functions. */
#undef syscall
#undef socketcall
#undef syscall_cp

/* The allocation fast path reads the per-thread theap through a pthread key.
 * Calling the public pthread_getspecific() on every malloc/free is expensive;
 * inline the exact same access.  `self->tsd` is guaranteed to be set whenever
 * a mimalloc key is valid (see the constructor comments below). */
#define MI_PTHREAD_GETSPECIFIC(key) ((void *)__pthread_self()->tsd[(key)])

/* ------------------------------------------------------------------------- */
/* mimalloc configuration                                                    */
/* ------------------------------------------------------------------------- */

#define MI_NO_GETENV 1
#define MI_PRIM_HAS_PROCESS_ATTACH 1

#include "static.c"

/* We must not fall back to the __thread based thread id helper: libc.so has
 * no usable PT_TLS module of its own. */
#if defined(MI_NO_THREAD_POINTER)
#error "musl+mimalloc requires a thread pointer primitive on this architecture"
#endif

#include "dynlink.h"
#include "fork_impl.h"

#define MUSL_EXPORT __attribute__((visibility("default")))

/* ------------------------------------------------------------------------- */
/* allocator entry points                                                    */
/* ------------------------------------------------------------------------- */

MUSL_EXPORT void *malloc(size_t n)
{
	return mi_malloc(n);
}

void *__libc_malloc(size_t n)
{
	return mi_malloc(n);
}

void *__libc_malloc_impl(size_t n)
{
	return mi_malloc(n);
}

void __libc_free(void *p)
{
	mi_free(p);
}

void *__libc_realloc(void *p, size_t n)
{
	return mi_realloc(p, n);
}

/* musl's calloc() would otherwise be malloc + memset.  Go through mi_calloc
 * so mimalloc can use its zero-initialized page paths. */
static void *musl_mimalloc_calloc(size_t m, size_t n)
{
	if (n != 0 && m > (size_t)-1 / n) {
		errno = ENOMEM;
		return NULL;
	}
	void *p = mi_calloc(m, n);
	if (!p && m != 0 && n != 0) errno = ENOMEM;
	return p;
}

MUSL_EXPORT void *calloc(size_t m, size_t n)
{
	return musl_mimalloc_calloc(m, n);
}

void *__libc_calloc(size_t m, size_t n)
{
	return musl_mimalloc_calloc(m, n);
}

MUSL_EXPORT void *aligned_alloc(size_t align, size_t len)
{
	if (align == 0 || (align & (align - 1)) != 0) {
		errno = EINVAL;
		return NULL;
	}
	void *p = mi_malloc_aligned(len, align);
	if (!p) errno = ENOMEM;
	return p;
}

MUSL_EXPORT size_t malloc_usable_size(void *p)
{
	return mi_usable_size(p);
}

hidden void __malloc_atfork(int who)
{
	(void)who;
}

hidden void __malloc_donate(char *a, char *b)
{
	/* musl's reclaim_gaps() only donates the unused sub-page gaps at the
	 * start/end of writable PT_LOAD segments (at most PAGE_SIZE-1 bytes per
	 * call).  mimalloc v3 can only take external OS memory through
	 * mi_manage_os_memory(), which requires at least MI_ARENA_MIN_SIZE
	 * (32 MiB on 64-bit, 8 MiB on 32-bit).  Accept such large donations if
	 * they ever appear, but silently ignore musl's normal small gaps. */
	uintptr_t start = (uintptr_t)a;
	uintptr_t end = (uintptr_t)b;
	const size_t align = mi_arena_min_alignment();
	const size_t min_size = mi_arena_min_size();

	start = (start + align - 1) & ~(uintptr_t)(align - 1);
	end &= ~(uintptr_t)(align - 1);
	if (end <= start || (size_t)(end - start) < min_size)
		return;

	/* reclaim_gaps() runs before the first allocation during dynamic
	 * linking; make sure the allocator is initialized before registering
	 * the range (mi_process_init() is idempotent). */
	mi_process_init();
	(void)mi_manage_os_memory((void *)start, (size_t)(end - start),
	                          true /* committed */,
	                          true /* pinned: never decommit */,
	                          false /* not known to be zero */,
	                          -1 /* numa node */);
}

/* ------------------------------------------------------------------------- */
/* process / thread lifecycle                                                */
/* ------------------------------------------------------------------------- */

__attribute__((constructor(101)))
static void mimalloc_process_attach(void)
{
	pthread_t self = __pthread_self();

	/* If mimalloc was initialised while the dynamic linker was loading
	 * libraries, it may have created pthread keys.  If the linker then
	 * replaced the static TLS block, the new struct pthread has a NULL tsd
	 * pointer and pthread_{get,set}specific() would fault.  musl itself
	 * avoids creating keys before that point; do the same repair that
	 * __pthread_key_create() would do.  (The key values themselves live in
	 * the process-global __pthread_tsd_main array and are not lost.) */
	if (!self->tsd) self->tsd = __pthread_tsd_main;

	mi_process_init();

	/* In the TLS-was-replaced case mimalloc's thread state lives in the
	 * pthread key values (which we kept reachable above).  The thread id,
	 * however, is derived from the thread pointer and therefore changed.
	 * It is the same kernel thread, so update the owning tld. */
	mi_theap_t *theap = _mi_theap_default();
	if (theap != NULL && mi_theap_is_initialized(theap)) {
		mi_thread_id_t tid = _mi_thread_id();
		if (theap->tld != NULL && theap->tld->thread_id != tid)
			theap->tld->thread_id = tid;
	}

	mi_thread_init();

	_mi_auto_process_init();
}

__attribute__((destructor(101)))
static void mimalloc_process_detach(void)
{
	_mi_auto_process_done();
}
