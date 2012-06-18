#!/bin/sh

GIT_URL=$1

if [ -z $GIT_URL ]; then
    echo "$0 GIT_URL"
    exit;
fi

ORIG=`pwd`
TEMPDIR=`mktemp -d -t madlib`
cd $TEMPDIR

git clone $GIT_URL madlib

# This is the version string for the distribution.
VERSION=`cat madlib/deploy/PGXN/dist.ver`

# For installation convention, add version string to the directory.
mv madlib madlib-$VERSION

m4 -DPGXN_VERSION=$VERSION madlib-$VERSION/deploy/PGXN/META.json_in > \
    madlib-$VERSION/META.json
zip -x "madlib-$VERSION/.git/*" -r $ORIG/madlib.zip ./madlib-$VERSION
rm -rf madlib-$VERSION

