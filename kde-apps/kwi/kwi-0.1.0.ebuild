# SPDX-FileCopyrightText: 2026 Kwi contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Distributed under the terms of the GNU General Public License v2

EAPI=8

ECM_HANDBOOK="optional"
ECM_TEST="true"
KFMIN=6.29.0
QTMIN=6.11.2
PYTHON_COMPAT=( python3_{12..15})

inherit ecm gear.kde.org xdg

DESCRIPTION="Modern and fast terminal emulator for KDE Plasma and other desktops"
HOMEPAGE="https://apps.kde.org/kwi"

LICENSE="|| ( GPL-2 GPL-3 ) || ( LGPL-2.1 LGPL-3 )"
SLOT="6"
KEYWORDS="~amd64 ~arm64 ~loong ~ppc64 ~riscv ~x86"
IUSE="+kpart"

DEPEND="
	>=dev-libs/kirigami-addons-1.11.0:6
	>=dev-libs/kirigami-app-components-1.1.1:6
	>=dev-libs/libghostty-vt-1.0.1:=
	>=dev-qt/qtbase-${QTMIN}:6[concurrent,dbus,gui,network,widgets]
	>=dev-qt/qtdeclarative-${QTMIN}:6
	>=dev-qt/qtsvg-${QTMIN}:6
	>=kde-frameworks/kcolorscheme-${KFMIN}:6
	>=kde-frameworks/kconfig-${KFMIN}:6
	>=kde-frameworks/kcoreaddons-${KFMIN}:6
	>=kde-frameworks/kdbusaddons-${KFMIN}:6
	>=kde-frameworks/kguiaddons-${KFMIN}:6
	>=kde-frameworks/ki18n-${KFMIN}:6
	>=kde-frameworks/kio-${KFMIN}:6
	>=kde-frameworks/kirigami-${KFMIN}:6
	>=kde-frameworks/knotifications-${KFMIN}:6
	>=kde-frameworks/kitemmodels-${KFMIN}:6
	>=kde-frameworks/kpty-${KFMIN}:6
	>=kde-frameworks/kwindowsystem-${KFMIN}:6[wayland]
	>=kde-frameworks/qqc2-desktop-style-${KFMIN}:6
	>=kde-plasma/union-6.8.3:6
	kpart? (
		>=dev-qt/qtdeclarative-${QTMIN}:6[widgets]
		>=kde-frameworks/kiconthemes-${KFMIN}:6
		>=kde-frameworks/kparts-${KFMIN}:6
		>=kde-frameworks/kwidgetsaddons-${KFMIN}:6
		>=kde-frameworks/kxmlgui-${KFMIN}:6
	)
"
RDEPEND="${DEPEND}"

BDEPEND="
	>=dev-qt/qtshadertools-${QTMIN}:6
	test? (
		${PYTHON_DEPS}
		sys-apps/dbus
	)
"

pkg_setup() {
	use test && python-any-r1_pkg_setup
}

src_configure() {
	local mycmakeargs=(
		-DBUILD_KPART=$(usex kpart)
	)

	ecm_src_configure
}
