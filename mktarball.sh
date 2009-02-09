#!/bin/sh

VERSION="$1"
if test "x$VERSION" = "x"; then
    echo "usage: $0 VERSION"
    exit 1
fi

workdir=`pwd`
stem="osm2go-$VERSION"
tarball="$stem.tar.gz"
tmpdir="/tmp/mktarball-$USER-$$"

echo "creating ../$tarball ..."
mkdir -p "$tmpdir"
ln -s "$workdir" "$tmpdir/$stem"
(cd "$tmpdir" && tar -c --exclude=.svn --dereference -pzf "$workdir/../$tarball" "$stem/")
rm -fr "$tmpdir"



