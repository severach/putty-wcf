# $Id: PKGBUILD 242505 2015-07-25 21:40:19Z eric $
# Maintainer:  Chris Severance aur.severach aATt spamgourmet dott com
# Contributor: Eric Bélanger <eric@archlinux.org

# This builds the Windows version of PuTTY.

# Build in Arch Linux with makepkg -scCf
# The .xz file contains nothing so erase it.
# PuTTY.EXE is delivered in the zip file.

set -u
_pkgname='putty'
pkgname="${_pkgname}-wcf"
pkgver=0.65
pkgrel=1
pkgdesc='A terminal integrated SSH/Telnet client for Windows (built on Linux)'
arch=('i686' 'x86_64')
url="http://www.chiark.greenend.org.uk/~sgtatham/${_pkgname}/"
license=('MIT')
depends=('gtk2')
makedepends=('mingw-w64-gcc' 'zip')
source=("http://the.earth.li/~sgtatham/putty/0.63/${_pkgname}-0.63.tar.gz"
        "http://the.earth.li/~sgtatham/putty/${pkgver}/${_pkgname}-${pkgver}.tar.gz")
#        putty-${pkgver}.tar.gz.sig::http://the.earth.li/~sgtatham/putty/latest/putty-${pkgver}.tar.gz.DSA)
sha256sums=('81e8eaaf31be7d9a46b4f3fb80d1d9540776f142cd89d0a11f2f8082dc68f8b5'
            'd543c1fd4944ea51d46d4abf31bfb8cde9bd1c65cb36dc6b83e51ce875660ca0')
#validpgpkeys=('00B1100938E698006518F0ABFECD6F3F08B0A90B')
_srcdir="${_pkgname}-${pkgver}/windows"

# Apply 0.63 changed files, produce 0.63 patch, hack patch, apply 0.63 patch to 0.65, produce clean 0.65 patch
# The folders are left in the patched state for more development
_fn_puttywcf063to065() {
  set -u
  local _patch="${startdir}/putty063wcf.patch"
  rm -f "${_patch}"
  cd "${srcdir}/putty-0.63"
  local _file
  for _file in $(cd "${startdir}/putty063wcf"; find -type f | sort); do
    _file="${_file#./}"
    if [ -s "${_file}" ]; then
      # Apply 0.63 changed files and generate 0.63 patch
      mv "${_file}" "${_file}.orig"
      cp -p "${startdir}/putty063wcf/${_file}" "${_file}"
      # -C1 causes part of my to-be-discarded patch to insert into putty.h in the wrong place. -C3 won't apply cleanly. -U seems to be the same as -C
      printf 'diff -r -C2 %q %q\n' "${_file}.orig" "${_file}" >> "${_patch}" # diff only outputs the diff lines on recurse. I like them!
      diff -C2 "${_file}.orig" "${_file}" >> "${_patch}" || :

      # Prepare backup for 0.65 from the 0.63wcf file list
      cp -p "${srcdir}/${_srcdir}/../${_file}" "${srcdir}/${_srcdir}/../${_file}.orig" # diff only backs up files where the patch doesn't apply cleanly. I need them all backed up.
    fi
  done

  # A quick one line fix will allow a large section in settings.c to patch automatically
  sed -e 's:wmap(sesskey, "Environment", conf, CONF_environmt:&, TRUE:g' "${_patch}" > "${_patch}.temp"
  cd "${srcdir}/${_srcdir}/.."
  cp -p "${srcdir}/putty-0.63/windows/winstore.c.orig" 'windows/winstore.c' # Revert this file from 0.65 back to 0.63. It only has a single minor patch that we've long since scooped!
  patch -p0 -i "${_patch}.temp" || :
  rm -f 'putty.h.rej' "${_patch}.temp" # SGT or MinGW fixed this so this part of my patch can be discarded.

  # Diff the 0.65 modified files for the next patch
  rm -rf "${startdir}/putty065.wcf"
  mkdir -p "${startdir}/putty065.wcf/windows"
  local _patch="${startdir}/putty065wcf.patch"
  rm -f "${_patch}"
  local _file
  for _file in $(find -type f -name '*.orig' | sort); do
    _file="${_file#./}"
    printf 'diff -r -C2 %q %q\n' "${_file}" "${_file%.orig}" >> "${startdir}/putty065wcf.patch"
    diff -C2 "${_file}" "${_file%.orig}" >> "${startdir}/putty065wcf.patch" || : # Same reverse as above
    cp -p "${_file%.orig}" "${startdir}/putty065.wcf/${_file%.orig}"
    rm -f  "${_file}"
  done
  set +u
}

prepare() {
  _fn_puttywcf063to065
  set -u
  cd "${srcdir}/${_srcdir}"
  # This switch was deprecated from Cygwin years ago. A quick fix and this makefile will build native Windows binaries with the mingw-w64 toolchain in Linux.
  sed -e 's:-mno-cygwin ::g' 'Makefile.cyg' > 'Makefile.win'
  # Brand our version WCF: WinClipFox, or Windows Clipboard and Foxpro support.
  sed -i -e 's:^#define TEXTVER "Release.*$:&" (WCF)":g' '../version.h'
  # Point to our website.
  sed -i -e 's@"http://www.chiark.greenend.org.uk/~sgtatham/putty/"@"https://github.com/severach/putty-wcf/"@g' 'windlg.c'
  set +u
  #false
}

build() {
  set -u
  cd "${_srcdir}"
  make -j $(nproc) -f 'Makefile.win' CC='i686-w64-mingw32-gcc' RC='i686-w64-mingw32-windres' putty.exe
  set +u
}

package() {
  set -u
  cd "${_srcdir}"
  rm -f "${startdir}/${pkgname}-${pkgver}.zip"
  zip -o "${startdir}/${pkgname}-${pkgver}.zip" *.exe
  set +u
}
set +u
