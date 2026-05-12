# Copyright 2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2


EAPI=8

EGO_PN="github.com/AdguardTeam/dnsproxy"

inherit git-r3 go-module

DESCRIPTION="Simple DNS proxy with DoH, DoT, DoQ, and DNSCrypt support"
HOMEPAGE="https://github.com/AdguardTeam/dnsproxy"
SRC_URI=""

EGIT_REPO_URI="https://github.com/AdguardTeam/dnsproxy.git"

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS=""
IUSE=""

DEPEND=">=dev-lang/go-1.26.0"
RDEPEND=""

BDEPEND="
	>=dev-lang/go-1.26
	>=dev-vcs/git-2
"

RESTRICT="mirror"

src_unpack() {
	git-r3_src_unpack
	go-module_live_vendor
}

src_compile() {

	# from ./scripts/make/go-build.sh
	export CGO_ENABLED=0
	local version_pkg='github.com/AdguardTeam/dnsproxy/internal/version'
	local -a ldflags=( -X "${version_pkg}.revision=${EGIT_VERSION}" )

	ego build \
		-ldflags="${ldflags[*]}" \
		-race=0 \
		-trimpath \
		-o="${PN}"
}

src_install() {
	dobin "${PN}"
	systemd_newunit "${FILESDIR}/dnsproxy.service-r1" dnsproxy.service
	insinto /etc/dnsproxy
	newins config.yaml.dist config.yaml
	newinitd "${FILESDIR}/dnsproxy.initd-r1" ${PN}
	insinto /etc/logrotate.d
	newins "${FILESDIR}/dnsproxy.logrotate-r1" ${PN}
}
