# Copyright 2025 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

inherit mount-boot

MY_PN=endfield-grub-theme

DESCRIPTION="Grub2 themes based on Arknights: Endfiled"
HOMEPAGE="https://github.com/AriharaShirone/endfield-grub-theme"
SRC_URI="https://github.com/AriharaShirone/${MY_PN}/archive/refs/tags/v${PV}.tar.gz -> ${P}.tgz"
S="${WORKDIR}/${MY_PN}-${PV}"
LICENSE="MIT CC-BY-SA-2.5"
SLOT="0"

KEYWORDS="amd64 arm arm64 ~loong ppc ppc64 ~riscv ~sparc x86"

PDEPEND="sys-boot/grub"

src_install() {
	rm README.md
	rm LICENSE
	local d
	for d in /boot /usr/share; do
		insinto "${d}"/grub/themes/endfield
		doins -r ./*
	done
}
