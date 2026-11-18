#!/usr/bin/env bash
# Writes the AUR package for a release into <out dir>: a PKGBUILD generated from pkgbuild/PKGBUILD that builds the
# release's source tarball instead of the working tree.
# Usage: tools/aur-pkgbuild.sh <version, e.g. 0.3.0> <sha256 of the source tarball> <out dir>
set -euo pipefail
cd "$(dirname "$0")/.."
version=$1 sha256=$2 out=$3
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "version must look like 0.3.0, got '$version'" >&2; exit 1; }
[[ $sha256 =~ ^[0-9a-f]{64}$ ]] || { echo "not a sha256: '$sha256'" >&2; exit 1; }
mkdir -p "$out"

# Drops the local-build header comment and pkgver() (with its comment), pins pkgver and the checksum, points _src
# and source at the release tarball and removes the git makedepend, which only pkgver() needed.
awk -v version="$version" -v sha256="$sha256" '
	BEGIN { print "# Maintainer: Koen Hendriks <info@koenhendriks.nl>"; print ""; header = 1 }
	header && !/^pkgname=/ { next }
	{ header = 0 }
	/^# Releases are tagged/ { skip = 1 }
	skip { if (/^}/) { skip = 0; dropblank = 1 } next }
	dropblank && /^$/ { dropblank = 0; next }
	{ dropblank = 0 }
	/^pkgver=/ { print "pkgver=" version; next }
	/^pkgrel=/ { print "pkgrel=1"; next }
	/^makedepends=/ { sub(/ \047git\047/, "") }
	/^_src=/ { print "_src=\"$pkgname-$pkgver\""; next }
	/^source=/ { print "source=(\"$url/releases/download/v$pkgver/$pkgname-$pkgver.tar.gz\")"; next }
	/^sha256sums=/ { print "sha256sums=(\047" sha256 "\047)"; next }
	{ print }
' pkgbuild/PKGBUILD >"$out/PKGBUILD"

check() { grep -q "$1" "$out/PKGBUILD" && { echo "$2 survived the conversion" >&2; exit 1; }; return 0; }
check '^pkgver()' 'pkgver()'
check 'startdir' 'the working-tree source'
check "'git'" 'the git makedepend'
grep -q "^pkgver=$version\$" "$out/PKGBUILD" || { echo "pkgver was not pinned" >&2; exit 1; }
grep -q "^sha256sums=('$sha256')\$" "$out/PKGBUILD" || { echo "the checksum was not pinned" >&2; exit 1; }
exit 0
