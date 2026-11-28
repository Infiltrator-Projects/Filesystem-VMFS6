#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Documentation-only package: no driver, executable, helper or maintainer script.
set -eu
name=VMFS6
slug=vmfs6
version=$(cat DOCUMENTATION_VERSION)
case "$version" in ''|*[!0-9.]*) exit 2;; esac
dpkg --validate-version "$version"
out=$(realpath -m "${1:-dist}")
mkdir -p "$out"
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT HUP INT TERM
pdf="docs/$name.pdf"
test -s "$pdf"
test "$(head -c 5 "$pdf")" = '%PDF-'
install -Dm644 "$pdf" "$stage/System/Resources/Documentation/Filesystem-$name.pdf"
mkdir -p "$stage/DEBIAN"
cat > "$stage/DEBIAN/control" <<EOF
Package: infiltrator-filesystem-$slug
Version: $version
Architecture: amd64
Maintainer: Infiltrator Projects
Section: doc
Priority: optional
Description: $name filesystem technical documentation (no runtime driver)
 This package contains only the filesystem reference PDF.
EOF
dpkg-deb --build --root-owner-group "$stage" "$out/infiltrator-filesystem-${slug}_${version}_amd64.deb"
