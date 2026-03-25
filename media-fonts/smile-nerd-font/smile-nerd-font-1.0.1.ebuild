# Copyright 2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8
inherit font

DESCRIPTION="Best programming language font that remix FiraCode Mono and LXGW Wenkai Mono"
HOMEPAGE="https://github.com/SOV710/smile-nerd-font"

IUSE="+regular +light"
LICENSE="OFL-1.1"

REQUIRED_USE="|| ( regular light )"

SRC_URI="light? ( https://github.com/SOV710/smile-nerd-font/releases/download/v${PV}/SmileNerdFontMono-Light.ttf -> ${P}-light.ttf )
		regular? ( https://github.com/SOV710/smile-nerd-font/releases/download/v${PV}/SmileNerdFontMono-Regular.ttf -> ${P}-regular.ttf )"

SLOT="0"

KEYWORDS="~alpha ~amd64 ~arm ~arm64 ~loong ~mips ~ppc ~ppc64 ~riscv ~sparc ~x86"

RESTRICT="binchecks strip"

FONT_SUFFIX="ttf"

S="${WORKDIR}"

DOCS=()

src_unpack(){
	cp ${DISTDIR}/${P}-* ${WORKDIR} || die
}

src_install() {
	font_src_install
}
