#!/bin/bash

if [ $# -gt 2 -o $# -lt 1 ]; then
	echo "Usage: $(basename ${0}) [curl_version] [icons.tar]" >&2
	exit 1
fi

CURL_VERSION=${1}
shift

set -ex

VERSION=$(sed -rn '/^project/s/.* VERSION +([^ ]+).*/\1/p' "$(dirname ${0})/CMakeLists.txt")
TMPDIR=$(mktemp -d)

mkdir ${TMPDIR}/osm2go-${VERSION}

# create a copy of the source dir, as we need to run CMake inside it
git archive --prefix=osm2go-${VERSION}/ ${VERSION} | tar x -C ${TMPDIR}

if [ $# -eq 1 ]; then
	tar -x -C ${TMPDIR}/osm2go-${VERSION} -f "${1}"
	find ${TMPDIR}/osm2go-${VERSION}/data -name '*.png' -exec touch {} +
fi

pushd ${TMPDIR}/osm2go-${VERSION}

tar xf ~/curl-${CURL_VERSION}.tar*
mv curl-${CURL_VERSION} curl

# run CMake so the debian/control is generated
cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_TOOLCHAIN_FILE=/opt/scratchbox.cmake .

fakeroot dpkg-buildpackage -us -uc

# cleanup
popd
mv ${TMPDIR}/osm2go_${VERSION}-maemo* .
rm -rf ${TMPDIR}
