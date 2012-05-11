#!/bin/sh

# This is the version string for the distribution
VERSION=0.3.0
TEMPDIR=`mktemp -d -t madlib`
ORIG=`pwd`
cd $TEMPDIR
git clone git://github.com/madlib/madlib.git madlib-$VERSION
touch madlib-$VERSION/PGXN_ZIP
zip -x "madlib-$VERSION/.git/*" -r $ORIG/madlib.zip ./madlib-$VERSION
rm -rf madlib-$VERSION

