#!/bin/bash

DATADIR=$(dirname ${0})

if [ "${1}" = "--xml" ]; then
	wget -O ${DATADIR}/defaultpresets.xml 'https://josm.openstreetmap.de/browser/josm/trunk/resources/data/defaultpresets.xml?format=txt'
	# clean up whitespace damage, allow multiple access values at once
	sed -r -i \
		-e 's/[[:space:]]*$//' \
		-e '/<combo key="(access|vehicle|bicycle|carriage|motor_vehicle|motorcycle|moped|mofa|motorcar|goods|hgv|bdouble|agricultural)"/s/,/;/g' \
		-e '/<combo key="(access|vehicle|bicycle|carriage|motor_vehicle|motorcycle|moped|mofa|motorcar|goods|hgv|bdouble|agricultural)"/s/<combo /<multiselect /g' \
		${DATADIR}/defaultpresets.xml
	# reset some nonsense that results from the previous sed command
	sed -i \
		-e '/<multiselect .*values="yes;designated;no"/s/;/,/g' \
		-e '/<multiselect .*values="yes,designated,no"/s/<multiselect /<combo /g' \
		${DATADIR}/defaultpresets.xml
	exit
fi

pushd ${DATADIR}

# cut the test number from CTest, this allows piping things directly from test output
sed 's/^[0-9]*:[[:space:]]//' | while read i; do
	mkdir -p icons/$(dirname ${i})
	wget -O icons/${i}.svg "https://josm.openstreetmap.de/svn/trunk/resources/images/${i}.svg"
	git add icons/${i}.svg
done
