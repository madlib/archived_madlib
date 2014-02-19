#!/bin/bash

# $0 - Script Path, $1 - Package Path, $2 - Target Location, and $3 - Target Volume

if [ -d "$2/usr/local/madlib/Versions" ]
then
    mv $2/usr/local/madlib/Versions $2/usr/local/madlib/Versions.bak
fi
