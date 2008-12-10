#!/bin/bash
#
# Copy all images from the given JOSM images path to the osm2go icon path
# and adjust their size
#
# Only process files that are actually referenced in presets.xml
# and elemstyles.xml
#

SIZE="24x24"
STYLE="styles/standard/"

if [ "$1" == "" ]; then
    echo "Usage: $0 JOSM_IMAGES_PATH"
    exit 0;
fi;

# cut trailing slash if present
CPATH=${1%/}

rm -rf $SUBDIRS

for f in `find $CPATH -iname '*.png'`; do
    NAME=.`echo $f | cut -b $[${#CPATH}+1]-`
    FOUND=""

    # search in presets.xml
    PRESETNAME=`echo $NAME | cut -b 3-`
    if [ "`grep $PRESETNAME ../presets.xml`" != "" ]; then
	echo "$NAME is referenced in presets.xml";
	FOUND="true"
    fi;
    
    # search in elemstyles.xml
    # elemstyle references the files without the leading
    # "styles/standard/"
    if [ "`echo $PRESETNAME | grep $STYLE`" != "" ]; then
	ELEMNAME=`echo $PRESETNAME | cut -b $[${#STYLE}+1]-`
	if [ "`grep $ELEMNAME ../elemstyles.xml`" != "" ]; then
	    echo "$NAME is referenced in elemstyles.xml";
	    FOUND="true"
	fi;
    fi;
    
    if [ "$FOUND" != "" ]; then
	mkdir -p `dirname $NAME`
	convert $f -scale $SIZE $NAME;
    else
	echo "Skipping unused $NAME"
    fi;
    
done;
