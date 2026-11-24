# Copyright 2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

ECM_TEST="false"
KFMIN="6.0.0"
QTMIN="6.6.0"
PLASMA_MIN="6.0.0"

inherit ecm

DESCRIPTION="KDE Plasma wallpaper plugin to play videos on your desktop"
HOMEPAGE="https://github.com/luisbocanegra/plasma-smart-video-wallpaper-reborn"
SRC_URI="https://github.com/luisbocanegra/${PN}/archive/v${PV}.tar.gz -> ${P}.tar.gz"

LICENSE="GPL-2+"
SLOT="6"
KEYWORDS="~amd64 ~arm64"

DEPEND="
	>=dev-qt/qtdeclarative-${QTMIN}:6
	>=kde-frameworks/kcmutils-${KFMIN}:6
	>=kde-frameworks/ki18n-${KFMIN}:6
	>=kde-plasma/knighttime-${PLASMA_MIN}:6
	>=kde-plasma/libplasma-${PLASMA_MIN}:6
"
RDEPEND="
	${DEPEND}
	>=dev-libs/glib-2.66:2
	>=dev-qt/qt5compat-${QTMIN}:6[qml]
	|| (
		>=dev-qt/qtmultimedia-${QTMIN}:6[qml,ffmpeg]
		>=dev-qt/qtmultimedia-${QTMIN}:6[qml,gstreamer]
	)
	>=kde-frameworks/kconfig-${KFMIN}:6[qml]
	>=kde-frameworks/kdeclarative-${KFMIN}:6
	>=kde-frameworks/kirigami-${KFMIN}:6
	>=kde-frameworks/ksvg-${KFMIN}:6
	>=kde-plasma/plasma-workspace-${PLASMA_MIN}:6
	>=kde-plasma/plasma5support-${PLASMA_MIN}:6
"
BDEPEND="sys-devel/gettext"

src_configure() {
	local mycmakeargs=(
		-DBUILD_PLUGIN=ON
		-DINSTALL_WALLPAPER=ON
		-DPACKAGE_WALLPAPER=OFF
	)

	cmake_src_configure
}
