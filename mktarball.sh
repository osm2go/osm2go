#!/bin/sh

VERSION="$1"
if [ -z "${VERSION}" ]; then
	VERSION=$(git describe --tags)
fi

git archive --format=tar -o osm2go-${VERSION}.tar --prefix=osm2go-${VERSION}/ HEAD
gzip --best osm2go-${VERSION}.tar
