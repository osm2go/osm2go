#!/bin/bash

DATADIR=$(dirname ${0})

if [ "${1}" = "--xml" ]; then
	wget -O ${DATADIR}/defaultpresets.xml 'https://josm.openstreetmap.de/browser/josm/trunk/resources/data/defaultpresets.xml?format=txt'
	sed 's/[[:space:]]*$//' -i ${DATADIR}/defaultpresets.xml
	exit
fi

pushd ${DATADIR}

# cut the test number from CTest, this allows piping things directly from test output
sed 's/^[0-9]*:[[:space:]]//' | while read i; do
	mkdir -p icons/$(dirname ${i})
	wget -O icons/${i}.svg "https://josm.openstreetmap.de/svn/trunk/resources/images/${i}.svg"
	git add icons/${i}.svg
done
