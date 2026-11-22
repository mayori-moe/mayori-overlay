# Copyright 2025 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

DESCRIPTION="Clean, Flat, Δ Theme for KDE Plasma Desktop"
HOMEPAGE="https://github.com/mayori-moe/cherry-kde"

if [[ "${PV}" == *9999* ]]; then
	inherit git-r3
	EGIT_REPO_URI="https://github.com/mayori-moe/${PN}.git"
else
	SRC_URI="https://github.com/mayori-moe/${PN}/archive/${PV}.tar.gz
		-> ${P}.tar.gz"
	KEYWORDS="~alpha ~amd64 ~arm ~arm64 ~hppa ~m68k ~mips ~ppc ~ppc64 ~s390 ~sparc ~x86"
fi

LICENSE="GPL-3"
SLOT="0"
IUSE="+aurorae konsole +kvantum wallpaper"

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
		insinto /usr/share/konsole
		doins konsole/cherry.colorscheme
		doins konsole/cherry-midnight.colorscheme
	fi

	if use kvantum; then
		insinto /usr/share/Kvantum
		for variant in cherry{,-solid,-midnight{,-solid}}; do
			doins -r kvantum/${variant}
		done
	fi

	if use wallpaper; then
		insinto /usr/share/wallpapers
		doins -r wallpaper/cherry
	fi
}
