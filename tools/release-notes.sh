#!/bin/sh
# Prints release notes for <tag>: the commits since the previous vX.Y.Z tag, install instructions and a compare link.
# Used by .github/workflows/release.yml; needs the full history (fetch depth 0).
# Usage: tools/release-notes.sh <tag> <compare URL prefix, e.g. https://github.com/owner/repo/compare>
set -eu
tag=$1 compare=$2
previous=$(git describe --tags --abbrev=0 --match 'v[0-9]*.[0-9]*.[0-9]*' "$tag^" 2>/dev/null || true)
range=${previous:+$previous..}$tag

echo "## Changes"
echo
git log --no-merges --pretty='- %s (%h)' "$range"
echo
echo "## Install"
echo
echo "Arch Linux and Omarchy: \`yay -S vox\` from the [AUR](https://aur.archlinux.org/packages/vox)."
echo
echo "From source: download \`vox-${tag#v}.tar.gz\` below or clone the repository, then run \`bin/install\`."
if [ -n "$previous" ]; then
	echo
	echo "**Full changelog:** $compare/$previous...$tag"
fi
