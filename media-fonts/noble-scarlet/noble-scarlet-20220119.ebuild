# Copyright 2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8
inherit font

DESCRIPTION="Deprecated but beautiful Noble Scarlet Font by Microsoft from Windows 10 Preview"
HOMEPAGE="https://learn.microsoft.com/typography/font-list"
SRC_URI="https://github.com/jlxd/Noble-Scarlet-mod/archive/642feea5cf672bf08471810192a0fa6c32049936.zip -> ${P}.zip"

LICENSE="BINARY-REDISTRIBUTABLE"

SLOT="0"
KEYWORDS="~alpha ~amd64 ~arm ~arm64 ~loong ~mips ~ppc ~ppc64 ~riscv ~sparc ~x86"

BDEPEND="app-arch/unzip"

RESTRICT="binchecks strip"

FONT_SUFFIX="ttc"

S="${WORKDIR}/Noble-Scarlet-mod-642feea5cf672bf08471810192a0fa6c32049936/"
DOCS=()

src_install() {
	mkdir install-hinted || die
	mv fonts/ms*.ttc install-hinted/. || die

	FONT_S="${S}/install-hinted/" font_src_install
}
