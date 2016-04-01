#!/bin/bash

# $0 - Script Path, $1 - Package Path, $2 - Target Location, and $3 - Target Volumn

MADLIB_VERSION=1.9

find $2/usr/local/madlib/bin -type d -exec cp -RPf {} $2/usr/local/madlib/old_bin \; 2>/dev/null
find $2/usr/local/madlib/bin -depth -type d -exec rm -r {} \; 2>/dev/null

find $2/usr/local/madlib/doc -type d -exec cp -RPf {} $2/usr/local/madlib/old_doc \; 2>/dev/null
find $2/usr/local/madlib/doc -depth -type d -exec rm -r {} \; 2>/dev/null

ln -nsf $2/usr/local/madlib/Versions/$MADLIB_VERSION $2/usr/local/madlib/Current
ln -nsf $2/usr/local/madlib/Current/bin $2/usr/local/madlib/bin
ln -nsf $2/usr/local/madlib/Current/doc $2/usr/local/madlib/doc

if [ -d "/usr/local/madlib/Versions.bak" ]
then
    mv -f $2/usr/local/madlib/Versions.bak/* $2/usr/local/madlib/Versions/
    rm -rf $2/usr/local/madlib/Versions.bak
fi
