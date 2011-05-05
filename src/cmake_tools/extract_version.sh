#!/bin/sh

VERSION=$(cut -c 10- Version.yml 2> /dev/null) || VERSION="UNKOWN"
echo ${VERSION}
