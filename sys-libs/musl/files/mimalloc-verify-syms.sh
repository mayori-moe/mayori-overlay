#!/bin/sh
# Verify symbol and TLS section hygiene for mimalloc-backed musl libc.
# Usage:
#   verify-syms.sh obj  <path/to/mimalloc.o>
#   verify-syms.sh dso  <path/to/libc.so>
#   verify-syms.sh ar   <path/to/libc.a>

set -eu

NM="${NM:-nm}"
READELF="${READELF:-readelf}"

mode="${1:-}"
target="${2:-}"

if [ -z "$mode" ] || [ -z "$target" ]; then
	echo "usage: $0 <obj|dso|ar> <file>" >&2
	exit 2
fi

fail() {
	echo "ERROR [$target]: $*" >&2
	exit 1
}

case "$mode" in
obj)
	# 1. Must not carry any TLS sections (.tbss / .tdata)
	tls_secs="$("$READELF" -SW "$target" | awk '$2 ~ /^\.(tbss|tdata)/ { print $2 }')"
	[ -z "$tls_secs" ] || fail "unexpected TLS sections in object: $tls_secs"

	# 2. Must not leak any global/weak mi_* or _mi_* symbols
	leaked="$("$NM" --defined-only -g "$target" | awk '$3 ~ /^_?mi_/ { print $3 }')"
	[ -z "$leaked" ] || fail "leaked mimalloc symbols in object: $leaked"

	# 3. Must not mark any public libc UND reference as STV_HIDDEN
	#    (which would hide that symbol in libc.so's .dynsym when linked)
	bad_und="$("$READELF" -sW "$target" | awk '$7 == "UND" && $6 == "HIDDEN" && $8 !~ /^__/ { print $8 }')"
	[ -z "$bad_und" ] || fail "public UND symbols poisoned with HIDDEN visibility: $bad_und"

	# 4. Verify all required public and internal symbols are defined
	defined="$("$NM" --defined-only -g "$target" | awk '{ print $3 }')"
	for sym in \
		malloc calloc realloc free aligned_alloc malloc_usable_size \
		__libc_malloc __libc_malloc_impl __libc_calloc __libc_realloc __libc_free \
		__malloc_donate __malloc_atfork
	do
		printf '%s\n' "$defined" | grep -qx "$sym" \
			|| fail "missing required symbol in object: $sym"
	done
	;;

dso)
	# 1. libc.so MUST NOT have a PT_TLS program header
	if "$READELF" -lW "$target" | awk '$1 == "TLS" { found=1 } END { exit !found }'; then
		fail "libc.so must not contain a PT_TLS segment"
	fi

	# 2. libc.so must not export any mi_* or _mi_* dynamic symbols
	leaked="$("$NM" -D --defined-only "$target" | awk '$3 ~ /^_?mi_/ { print $3 }')"
	[ -z "$leaked" ] || fail "leaked mimalloc dynamic symbols in libc.so: $leaked"

	# 3. Verify standard allocator and key libc exports are intact
	exported="$("$NM" -D --defined-only "$target" | awk '{ print $3 }')"
	for sym in \
		malloc calloc realloc free aligned_alloc memalign posix_memalign \
		reallocarray malloc_usable_size \
		environ getauxval getrusage sysinfo prctl
	do
		printf '%s\n' "$exported" | grep -qx "$sym" \
			|| fail "missing required dynamic symbol in libc.so: $sym"
	done
	;;

ar)
	# libc.a must not leak any global/weak mi_* or _mi_* symbols
	leaked="$("$NM" --defined-only -g "$target" | awk '$3 ~ /^_?mi_/ { print $3 }')"
	[ -z "$leaked" ] || fail "leaked mimalloc symbols in libc.a: $leaked"
	;;

*)
	fail "unknown mode: $mode"
	;;
esac
