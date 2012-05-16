#!/bin/sh

GIT_URL=$1

if [ -z $GIT_URL ]; then
    echo "$0 GIT_URL"
    exit;
fi

# This is the version string for the distribution
TEMPDIR=`mktemp -d -t madlib`
ORIG=`pwd`
PGXNDIR=`dirname $0`
PGXNDIR=`(cd $PGXNDIR; pwd)`
VERSION=`cat $PGXNDIR/dist.ver`
cd $TEMPDIR

git clone $GIT_URL madlib-$VERSION

m4 -DPGXN_VERSION=$VERSION $PGXNDIR/META.json_in > madlib-$VERSION/META.json
zip -x "madlib-$VERSION/.git/*" -r $ORIG/madlib.zip ./madlib-$VERSION
rm -rf madlib-$VERSION

