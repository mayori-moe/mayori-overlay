# Copyright 1999-2025 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

COMMIT="abb3163c724935af888ba5ea9ac0c4f22afd8048"

DESCRIPTION="Series of modern looking themes for SDDM."
HOMEPAGE="https://github.com/Keyitdev/sddm-astronaut-theme"
SRC_URI="${HOMEPAGE}/archive/${COMMIT}.zip"

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="*"
RESTRICT="bindist mirror"

DEPEND="
	dev-qt/qtmultimedia
	dev-qt/qtsvg
	dev-qt/qtvirtualkeyboard
	gui-apps/sddm
"
RDEPEND=${DEPEND}
BDEPEND=""

QA_PREBUILD="*"

S="${WORKDIR}/${PN}-${COMMIT}"

src_install() {
	mkdir -p "${ED}/usr/share/sddm/themes/${PN}" || die "mkdir failed"
	cp -a * "${ED}/usr/share/sddm/themes/${PN}" || die "cp failed"
	mkdir -p "${ED}/usr/share/fonts" || die "mkdir failed"
	cp -ar "Fonts/"* "${ED}/usr/share/fonts" || die "cp failed"
}
