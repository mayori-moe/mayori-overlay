# Copyright 2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

inherit qmake-utils xdg

MY_PV="${PV/_beta/-beta}"
MY_P="${PN}-${MY_PV}"

QTW_PN="qmltermwidget"
QTW_PV=8913504fa2ebd220ebe7c680c32954e1b3c035c5
QTW_P="${QTW_PN}-${QTW_PV}"

KDSA_PN="KDSingleApplication"
KDSA_PV=1848dd64e80e37326da4a22b2c1f45f2a1c4f231
KDSA_P="${KDSA_PN}-${KDSA_PV}"

DESCRIPTION="Terminal emulator with an old school look and feel"
HOMEPAGE="https://github.com/Swordfish90/cool-retro-term/"

SRC_URI="
		https://github.com/Swordfish90/cool-retro-term/archive/${MY_PV}.tar.gz -> ${MY_P}.tar.gz
		https://github.com/Swordfish90/qmltermwidget/archive/${QTW_PV}.tar.gz -> ${QTW_P}.tar.gz
		https://github.com/KDAB/KDSingleApplication/archive/${KDSA_PV}.tar.gz -> ${KDSA_P}.tar.gz
"

S="${WORKDIR}/${MY_P}"

LICENSE="BSD MIT GPL-2+ GPL-3+"
SLOT="0"
KEYWORDS="~amd64"

DEPEND="
		dev-qt/qtbase:6
		dev-qt/qtdeclarative:6[widgets]
		dev-qt/qt5compat:6
"
RDEPEND="${DEPEND}
		virtual/opengl"

src_prepare() {
	default

	rmdir "${QTW_PN}" || die
	mv "${WORKDIR}/${QTW_P}" "${QTW_PN}" || die

	rmdir "${KDSA_PN}" || die
	mv "${WORKDIR}/${KDSA_P}" "${KDSA_PN}" || die
}

src_configure() {
	eqmake6 PREFIX="${EPREFIX}/usr"
}

src_install() {
	# `default` attempts to install directly to /usr and parallelised
	# installation is not supported as `qmake6 -install` does not implictly
	# create target directory.

	emake INSTALL_ROOT="${ED}" install
	doman "packaging/debian/cool-retro-term.1"

	insinto "/usr/share/metainfo"
	doins "packaging/appdata/cool-retro-term.appdata.xml"
}
