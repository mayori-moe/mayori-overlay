/*
 * Glue translation unit for embedding mimalloc (v3.7+) into musl libc via
 * --with-malloc=external without modifying upstream mimalloc sources.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Configure mimalloc for embedding directly inside musl libc */
#define MI_LIBC_MUSL               1
#define MI_TLS_MODEL_FIXED         1
#define MI_TLS_MODEL_FIXED_DEFAULT 0
#define MI_TLS_MODEL_FIXED_CACHED  1
#define MI_NO_CXX_NEW_HANDLER      1
#define MI_NO_PTHREAD_ATFORK       1
#define MI_NO_GETENV               1
#define MI_PRIM_HAS_PROCESS_ATTACH 1
#define MI_SKIP_COLLECT_ON_EXIT    1
#define MI_PROFILE                 0
#define MI_STATS                   0
#define MI_BUILD_RELEASE           1
#ifndef NDEBUG
#define NDEBUG                     1
#endif

#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* musl internal headers */
#include "dynlink.h"
#include "fork_impl.h"
#include "libc.h"
#include "pthread_impl.h"

/*
 * Undefine musl internal convenience macros that collide with identifiers
 * inside upstream mimalloc (e.g., mi_random_ctx_t::weak, syscall()).
 */
#undef weak
#undef syscall

/*
 * Fast fixed per-thread heap slots stored directly in struct pthread
 * (non-ABI Part 2, cache-line 0 alongside self/canary/tid/errno_val).
 * Preserved across ldso's initial_tls replacement in __init_tp().
 */
#define MI_PRIM_THREAD_POINTER() (__pthread_self()->malloc_tls)

/*
 * Stable per-thread identifier for mimalloc:
 *
 * - Cannot use self->tid: PID 1 in containers has tid == 1, which violates
 *   (tid > MI_THREAD_ID_DETACHED) and ((tid & 3) == 0); tid also changes on
 *   fork().
 * - Cannot use (uintptr_t)__pthread_self(): ldso replaces builtin_tls with
 *   initial_tls in dynlink.c, which changes the main thread's struct pthread
 *   address after early allocations have already occurred.
 * - __pthread_self()->tsd is initialized to __pthread_tsd_main in __init_tp()
 *   for the main thread (stable across initial_tls replacement and fork()),
 *   and to a unique >=16-byte-aligned per-thread array in pthread_create.c for
 *   every worker thread. Its address is always > MI_THREAD_ID_DETACHED and has
 *   the bottom 2 bits clear.
 * - On x86_64 (!TLS_ABOVE_TP), struct pthread begins at %fs:0, so reading via
 *   __seg_fs compiles to a single `xorq %fs:ofs, %reg` in mi_free().
 */
#if defined(__x86_64__) && defined(__SEG_FS) && !defined(TLS_ABOVE_TP)
#define MI_PRIM_THREAD_ID() \
	((mi_thread_id_t)(uintptr_t)(*(void *const __seg_fs *)offsetof(struct pthread, tsd)))
#else
#define MI_PRIM_THREAD_ID() ((mi_thread_id_t)(uintptr_t)__pthread_self()->tsd)
#endif

/* Inline pthread_getspecific() for auxiliary thread-local lookups */
#define pthread_getspecific(k) ((void *)__pthread_self()->tsd[(k)])

/* Bind internal pthread operations directly to musl's hidden entrypoints */
#define pthread_key_create    __pthread_key_create
#define pthread_key_delete    __pthread_key_delete
#define pthread_mutex_lock    __pthread_mutex_lock
#define pthread_mutex_trylock __pthread_mutex_trylock
#define pthread_mutex_unlock  __pthread_mutex_unlock

/*
 * Include upstream mimalloc unity build WITHOUT visibility push(hidden), so
 * system headers/externs included by mimalloc (environ, getauxval, getrusage,
 * prctl, sysinfo, ...) keep DEFAULT visibility and do not hide libc.so exports.
 * Internal mimalloc definitions are localized afterwards via objcopy
 * --keep-global-symbol=... (and intra-TU calls avoid PLT/GOT via
 * -fno-semantic-interposition).
 *
 * Mirrors mimalloc/src/static.c while scoping MI_TLS_MODEL_PTHREADS strictly
 * around util/threadlocal.c so its auxiliary slots do not emit __thread (.tbss)
 * in libc.so while the hot allocation path uses MI_TLS_MODEL_FIXED.
 */
#include "mimalloc.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#endif

#include "prim/os.c"
#include "prim/prim.c"
#include "prim/prim-tls.c"

#include "util/libc.c"
#include "util/memid.c"
#include "util/options.c"
#include "util/random.c"
#include "util/stats.c"

#define MI_TLS_MODEL_PTHREADS 1
#include "util/threadlocal.c"
#undef MI_TLS_MODEL_PTHREADS

#include "arena/arena.c"
#include "arena/arena-alloc.c"
#include "arena/arena-page.c"
#include "arena/arena-page-alloc.c"
#include "arena/arena-heap.c"
#include "arena/bitmap-chunk.c"
#include "arena/bitmap.c"

#include "alloc/alloc.c"
#include "alloc/alloc-generic.c"
#include "alloc/alloc-aligned.c"
#include "alloc/alloc-posix.c"
#include "alloc/page.c"
#include "alloc/page-queue.c"

#include "heap/heap.c"
#include "heap/process.c"
#include "heap/page-map.c"
#include "heap/subproc.c"
#include "heap/theap.c"

#include "profile/sample-guarded.c"
#include "profile/sample-profile.c"
#include "profile/pprof.c"
#include "profile/pprof-os.c"
#include "profile/pprof-write.c"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

/* Restore macro names for any following code */
#undef pthread_getspecific
#undef pthread_key_create
#undef pthread_key_delete
#undef pthread_mutex_lock
#undef pthread_mutex_trylock
#undef pthread_mutex_unlock

/*
 * Process attach hook (replaces mimalloc's default constructor/destructor).
 * Deliberately omit a destructor: as the system libc, libc.so is never
 * unloaded before _Exit(), and musl's __stdio_exit() runs after .fini_array.
 */
static void __attribute__((constructor(101))) mimalloc_process_attach(void)
{
	_mi_auto_process_init();
	__pthread_self()->tsd_used = 1;
}

/* --------------------------------------------------------------------------
 * musl internal hidden allocator entrypoints
 * Direct ELF aliases avoid extra jmp thunks and duplicate inlined code while
 * preserving mimalloc's mi_decl_func_align alignment on every hot entry.
 * -------------------------------------------------------------------------- */

#define mimalloc_alias_hidden(target, name) \
	extern __typeof(target) name __attribute__((__alias__(#target), __visibility__("hidden")))

#define mimalloc_export_weak(target, name) \
	extern __typeof(target) name __attribute__((__weak__, __alias__(#target), __visibility__("default")))

mimalloc_alias_hidden(mi_malloc,  __libc_malloc_impl);
mimalloc_alias_hidden(mi_malloc,  __libc_malloc);
mimalloc_alias_hidden(mi_calloc,  __libc_calloc);
mimalloc_alias_hidden(mi_realloc, __libc_realloc);
mimalloc_alias_hidden(mi_free,    __libc_free);

hidden void __malloc_donate(char *start, char *end)
{
	(void)start;
	(void)end;
}

hidden void __malloc_atfork(int who)
{
	if (who > 0 && _mi_process.process_is_initialized) {
		_mi_fork_child();
	}
}

/* --------------------------------------------------------------------------
 * Public interposable allocator entrypoints (weak + default visibility so
 * both dynamic interposition and static allocator replacement work cleanly)
 * -------------------------------------------------------------------------- */

static __attribute__((__noinline__, __cold__))
void *mimalloc_calloc_replaced(size_t m, size_t n, void *(*fn)(size_t))
{
	if (n && m > (size_t)-1 / n) {
		errno = ENOMEM;
		return 0;
	}
	n *= m;
	void *p = fn(n);
	if (!p) return 0;
	return memset(p, 0, n);
}

static mi_decl_func_align void *mimalloc_calloc(size_t m, size_t n)
{
	void *(*cur_malloc)(size_t) = malloc;
	__asm__("" : "+r"(cur_malloc));
	if (__builtin_expect(__malloc_replaced, 0) ||
	    __builtin_expect(cur_malloc != __libc_malloc_impl, 0))
		return mimalloc_calloc_replaced(m, n, cur_malloc);
	return mi_calloc(m, n);
}

static __attribute__((__noinline__, __cold__))
void *mimalloc_aligned_alloc_replaced(void)
{
	errno = ENOMEM;
	return 0;
}

static mi_decl_func_align void *mimalloc_aligned_alloc(size_t align, size_t len)
{
	if (__builtin_expect(__malloc_replaced && !__aligned_alloc_replaced, 0))
		return mimalloc_aligned_alloc_replaced();
	return mi_aligned_alloc(align, len);
}

mimalloc_export_weak(mi_malloc,              malloc);
mimalloc_export_weak(mimalloc_calloc,        calloc);
mimalloc_export_weak(mi_realloc,             realloc);
mimalloc_export_weak(mi_free,                free);
mimalloc_export_weak(mimalloc_aligned_alloc, aligned_alloc);
extern size_t malloc_usable_size(void *)
	__attribute__((__weak__, __alias__("mi_usable_size"), __visibility__("default")));
