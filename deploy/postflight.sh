#!/bin/bash

# $0 - Script Path, $1 - Package Path, $2 - Target Location, and $3 - Target Volumn

MADLIB_VERSION=1.4

find /usr/local/madlib/bin -type d -exec cp -RPf {} /usr/local/madlib/old_bin \; 2>/dev/null
find /usr/local/madlib/bin -depth -type d -exec rm -r {} \; 2>/dev/null

find /usr/local/madlib/doc -type d -exec cp -RPf {} /usr/local/madlib/old_doc \; 2>/dev/null
find /usr/local/madlib/doc -depth -type d -exec rm -r {} \; 2>/dev/null

#ln -sf $2 /usr/local/madlib/Current
ln -nsf /usr/local/madlib/Versions/$MADLIB_VERSION /usr/local/madlib/Current
ln -nsf /usr/local/madlib/Current/bin /usr/local/madlib/bin
ln -nsf /usr/local/madlib/Current/doc /usr/local/madlib/doc

if [ -d "/usr/local/madlib/Versions.bak" ]
then
    mv -f /usr/local/madlib/Versions.bak/* /usr/local/madlib/Versions/
    rm -rf /usr/local/madlib/Versions.bak
fi
