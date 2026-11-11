# Copyright 2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

inherit toolchain-funcs

DESCRIPTION="glibc ABI shim and musl runtime pieces for NVIDIA user-space tools"
HOMEPAGE="https://github.com/12101111/overlay"
SRC_URI="
	https://dl-cdn.alpinelinux.org/alpine/latest-stable/main/x86_64/libstdc++-15.2.0-r5.apk
		-> libstdc++-15.2.0-r5.tar.gz
"
LICENSE="MIT GPL-2+ LGPL-2.1+"
# Same sub-slot contract as the current package: nvidia-drivers depends on
# it with := and is rebuilt when the shim ABI or private layout changes.
SLOT="0/1"
KEYWORDS="~amd64"
IUSE=""

# fts_open/fts_read/... are used by nvidia-installer and are provided by
# the standalone musl fts implementation on this system.
DEPEND="sys-libs/fts-standalone"
# Legacy variant: the private libstdc++ is pointed at libunwind instead of
# libgcc, and the shim provides the three compiler-runtime helpers.
# libgbm.so.1 always comes from the system Mesa, like the current package.
RDEPEND="${DEPEND}
	llvm-runtimes/libunwind
	media-libs/mesa
"
BDEPEND="dev-util/patchelf"

S="${WORKDIR}"

QA_PREBUILT="
	usr/lib/nvidia-musl-compat/libstdc++.so.6.0.34
"
QA_DT_NEEDED=( "libc[.]musl-x86_64[.]so[.]1" )

src_compile() {
	$(tc-getCC) ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -shared -fPIC \
		-o libglibc-compat.so.1 "${FILESDIR}"/shim.c \
		-Wl,-soname,libglibc-compat.so.1 -lfts || die
}

src_install() {
	# ---- the shim itself ------------------------------------------------
	insinto /usr/lib/nvidia-musl-compat
	doins libglibc-compat.so.1
	dosym libglibc-compat.so.1 /usr/lib/nvidia-musl-compat/libglibc-compat.so

	# Every patched NVIDIA ELF has DT_NEEDED=libglibc-compat.so.1.  musl's
	# default search path contains /usr/lib, so plain symlinks suffice.
	dosym nvidia-musl-compat/libglibc-compat.so.1 /usr/lib/libglibc-compat.so.1
	dosym nvidia-musl-compat/libglibc-compat.so /usr/lib/libglibc-compat.so

	# ---- musl-native C++ runtime (legacy: libunwind, no libgcc) ---------
	# Keep it in a private directory and never claim /usr/lib/libstdc++.so.6.
	if ! has_version 'sys-devel/gcc'; then
		insinto /usr/lib/nvidia-musl-compat
		doins "${WORKDIR}"/usr/lib/libstdc++.so.6.0.34
		dosym libstdc++.so.6.0.34 /usr/lib/nvidia-musl-compat/libstdc++.so.6
		# Redirect the compiler runtime to libunwind; the three helper
		# symbols come from shim.c.
		patchelf --replace-needed libgcc_s.so.1 libunwind.so.1 \
			"${ED}"/usr/lib/nvidia-musl-compat/libstdc++.so.6.0.34 || die
	fi

	# ---- loader name compatibility --------------------------------------
	dodir /lib64
	dosym ld-musl-x86_64.so.1 /lib/libc.musl-x86_64.so.1
	dosym ld-musl-x86_64.so.1 /lib/ld-linux-x86-64.so.2
	dosym ../lib/ld-musl-x86_64.so.1 /lib64/ld-linux-x86-64.so.2
}
