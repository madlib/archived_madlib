#!/bin/bash

# $0 - Script Path, $1 - Package Path, $2 - Target Location, and $3 - Target Volumn

if [ -d "/usr/local/madlib/Versions" ]
then
    mv /usr/local/madlib/Versions /usr/local/madlib/Versions.bak
fi
