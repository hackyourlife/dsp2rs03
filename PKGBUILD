pkgname='dsp2rs03'
pkgdesc='devkitDSP to Retro Studios RS03 converter'
pkgver=r3.ga5e0c50
pkgrel=1
url="https://github.com/hackyourlife/dsp2rs03"
license=(GPL)
arch=(x86_64 aarch64)
source=('dsp2rs03.c')
sha256sums=('SKIP')

pkgver() {
	echo "r$(git rev-list --count HEAD).g$(git rev-parse --short HEAD)"
}

build() {
	cd "${srcdir}"

	gcc -O3 -o dsp2rs03 dsp2rs03.c
}

package() {
	install -dm755 "${pkgdir}/usr/bin"
	install -m755 "${srcdir}/dsp2rs03" "${pkgdir}/usr/bin"
}
