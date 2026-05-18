# Copyright 2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

inherit go-module systemd

DESCRIPTION="Simple DNS proxy with DoH, DoT, DoQ and DNSCrypt support"
HOMEPAGE="https://github.com/AdguardTeam/dnsproxy"

SRC_URI="
	https://github.com/AdguardTeam/dnsproxy/archive/refs/tags/v${PV}.tar.gz -> ${P}.tar.gz
	https://github.com/mayori-moe/mayori-overlay/releases/download/distfiles/dnsproxy-0.81.3-vendor.tar.gz
"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="~amd64 ~arm ~arm64 ~ppc64 ~x86"

BDEPEND=">=dev-lang/go-1.26.0"

src_compile() {
	# from ./scripts/make/go-build.sh
	export CGO_ENABLED=0
	local version_pkg='github.com/AdguardTeam/dnsproxy/internal/version'
	local -a ldflags=( -X "${version_pkg}.version=${PV}" )
	ego build \
		-ldflags="${ldflags[*]}" \
		-race=0 \
		-trimpath \
		-o="${PN}"
}

src_install() {
	dobin "${PN}"
	insinto /etc/dnsproxy
	newins config.yaml.dist config.yaml
	systemd_newunit "${FILESDIR}/dnsproxy.service-r1" dnsproxy.service
	newinitd "${FILESDIR}/dnsproxy.initd-r1" ${PN}
	insinto /etc/logrotate.d
	newins "${FILESDIR}/dnsproxy.logrotate" ${PN}
}
