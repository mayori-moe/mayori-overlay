# Copyright 2025 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

DESCRIPTION="Clean, Flat, Δ Theme for KDE Plasma Desktop"
HOMEPAGE="https://github.com/nullxception/cherry-kde"

EGIT_REPO_URI="https://github.com/nullxception/cherry-kde.git"
inherit git-r3

LICENSE="GPL-3.0-only"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="aurorae konsole +kvantum wallpaper"

DEPEND=""
RDEPEND="
	kde-plasma/plasma-desktop
	konsole? ( kde-apps/konsole )
	kvantum? ( x11-themes/kvantum )
"

src_install() {
	insinto /usr/share/plasma/desktoptheme
	for variant in cherry{,-midnight}; do
		doins -r plasma/desktoptheme/${variant}
	done

	insinto /usr/share/plasma/look-and-feel
	local namespace=com.github.nullxception
	for variant in cherry{,midnight}; do
		doins -r plasma/look-and-feel/${namespace}.${variant}
	done

	insinto /usr/share/color-schemes
	doins color-schemes/Cherry.colors
	doins color-schemes/CherryMidnight.colors

	if use aurorae; then
		insinto /usr/share/aurorae/themes
		for variant in cherry{,-solid,-square{,-solid},-midnight{,-solid,-square{,-solid}}}; do
			doins -r aurorae/${variant}
		done
	fi

	if use konsole; then
		insinto /usr/share/konsole/color-schemes
		doins color-schemes/Cherry.colors
		doins colors-schemes/CherryMidnight.colors
	fi

	if use kvantum; then
		insinto /usr/share/Kvantum
		for variant in cherry{,-midnight}; do
			doins -r plasma/desktoptheme/${variant}
		done
	fi

	if use wallpaper; then
		insinto /usr/share/wallpapers
		doins -r wallpaper/cherry
	fi
}
