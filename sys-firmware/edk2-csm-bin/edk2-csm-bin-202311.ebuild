# Copyright 1999-2025 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

inherit readme.gentoo-r1

DESCRIPTION="TianoCore EDK II UEFI firmware with CSM support from Arch Linux archive"
HOMEPAGE="https://github.com/tianocore/edk2"
SRC_URI="https://archive.archlinux.org/packages/e/edk2-ovmf/edk2-ovmf-202311-1-any.pkg.tar.zst -> ${P}.tar.zst"
S="${WORKDIR}"
LICENSE="BSD-2-with-patent MIT"
SLOT="0"
KEYWORDS="amd64 arm64 ~loong ~ppc ppc64 ~riscv x86"

BDEPEND="app-arch/zstd"

RDEPEND="|| ( sys-firmware/edk2-bin sys-firmware/edk2 )"


src_unpack() {
	tar -xvf "${DISTDIR}/${P}.tar.zst" "usr/share/edk2/x64/OVMF_CODE.csm.4m.fd" "usr/share/edk2/x64/OVMF_VARS.4m.fd" "usr/share/qemu/firmware/70-edk2-ovmf-x86_64-csm-4m.json"|| die
}

src_prepare() {
	mv usr/share/edk2/x64 usr/share/edk2/OvmfX64
	mv usr/share/edk2/OvmfX64/OVMF_CODE.csm.4m.fd usr/share/edk2/OvmfX64/OVMF_CODE.csm.fd
	mv usr/share/edk2/OvmfX64/OVMF_VARS.4m.fd usr/share/edk2/OvmfX64/OVMF_VARS.csm.fd
	mv usr/share/qemu/firmware/70-edk2-ovmf-x86_64-csm-4m.json usr/share/qemu/firmware/70-edk2-ovmf-x86_64-csm.json
	sed -i 's/x64\/OVMF_CODE\.csm\.4m\.fd/OvmfX64\/OVMF_CODE.csm.fd/g' usr/share/qemu/firmware/70-edk2-ovmf-x86_64-csm.json
	sed -i 's/x64\/OVMF_VARS\.4m\.fd/OvmfX64\/OVMF_VARS.csm.fd/g' usr/share/qemu/firmware/70-edk2-ovmf-x86_64-csm.json
	default
}

src_install() {
	insinto /usr/share
	doins -r usr/share/{edk2,qemu}/
}

pkg_postinst() {
	readme.gentoo_print_elog
}
