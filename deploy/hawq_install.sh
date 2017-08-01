#!/bin/bash

## ########################################################################
## MADLIb install script for HAWQ as a replacement for "gppkg -i"
## Possible improvements:
##   1. confirm ${GPHOME} with user
##   2. use rpm --test to check before installing
## ########################################################################

COMMAND=`basename $0`
USAGE="COMMAND NAME: ${COMMAND}

MADlib install script for HAWQ as a replacement for \"gppkg -i\".


*****************************************************
SYNOPSIS
*****************************************************

${COMMAND} -r <RPM_FILEPATH> -f <HOSTFILE>
    [-s] [-d <GPHOME>] [--prefix <MADLIB_INSTALL_PATH>]

${COMMAND} -h


*****************************************************
PREREQUISITES
*****************************************************

The following tasks should be performed prior to executing this script:

* Make sure you have the necessary binaries available in your PATH:
  rpm, gpssh, gpscp.
* Make sure the provided HOSTFILE contains all segment nodes.
* Run with -s option is you do not want to install/reinstall on localhost.
* Set GPHOME to the correct HAWQ installation directory, either by
  environment variable or option -d


*****************************************************
OPTIONS
*****************************************************

-r | --rpm-path <RPM_FILEPATH>
 Required. Path to the MADlib RPM file.

-f | --host-file <HOSTFILE>
 Required. Hostfile contains host names of all segment nodes.

-d | --set-gphome <GPHOME>
 Optional. Installation path of HAWQ. If not set, the script will try to
 use the environment variable.

--prefix <MADLIB_INSTALL_PATH>
 Optional. Expected MADlib installation path. If not set, the default value
 \${GPHOME}/madlib is used.

-s | --skip-localhost
 Optional. If not set, the RPM file will be installed to localhost as well
 as hosts in HOSTFILE.

-h | -? | --help
 Displays the online help.


*****************************************************
EXAMPLE
*****************************************************

/home/gpadmin/madlib/${COMMAND} \\
    -r /home/gpadmin/madlib/madlib-1.5-Linux.rpm \\
    -f /usr/local/greenplum-db/hostfile
"
HELP="Try option -h for detailed usage."

## ########################################################################
## parsing command-line args
if [ $# -lt 1 ]; then
    echo "$0: Missing required arguments. ${HELP}"
    exit 1
fi
SKIP_LOCALHOST=0
until [ -z $1 ]
do
    case "$1" in
        --prefix)             MADLIB_INSTALL_PATH=$2; shift;;
        -r|--rpm-path)        RPM_FILEPATH=$2; shift;;
        -f|--host-file)       HOSTFILE=$2; shift;;
        -d|--set-gphome)      GPHOME=$2; shift;;
        -s|--skip-localhost)  SKIP_LOCALHOST=1; shift;;
        -h|-?|--help)
            echo "${USAGE}";
            exit 1;;
        *)
            echo "$0: No such option $1. ${HELP}";
            exit 1;;
    esac
    shift
done
# checking and verbosing options
if [ ${GPHOME} ]; then
    echo "Using HAWQ installation: ${GPHOME}"
else
    echo "$0: GPHOME is not set or empty. ${HELP}"
    exit 1
fi
if [ ${RPM_FILEPATH} ]; then
    echo "Using RPM file: ${RPM_FILEPATH}"
else
    echo "$0: RPM_FILEPATH is not set or empty. ${HELP}"
    exit 1
fi
if [ ${HOSTFILE} ]; then
    echo "Using MADlib installation path: ${HOSTFILE}"
else
    echo "$0: HOSTFILE is not set or empty. ${HELP}"
    exit 1
fi
if [ ${MADLIB_INSTALL_PATH} ]; then
    echo "Using MADlib installation path: ${MADLIB_INSTALL_PATH}"
else
    MADLIB_INSTALL_PATH="${GPHOME}"
fi

## ########################################################################
## checking necessary binaries
which rpm > /dev/null
if [ 0 -ne $? ]; then
    echo "$0: rpm not found in PATH. ${HELP}"
    exit 1
fi
which gpssh > /dev/null
if [ 0 -ne $? ]; then
    echo "$0: gpssh not found in PATH. ${HELP}"
    exit 1
fi
which gpscp > /dev/null
if [ 0 -ne $? ]; then
    echo "$0: gpscp not found in PATH. ${HELP}"
    exit 1
fi

## ########################################################################
## checking and erasing any installed matching-version RPMs
RPM=`basename ${RPM_FILEPATH}`
RPM_PACKAGE=`echo ${RPM} | sed -e s/-Linux// | sed -e s/\.rpm//`
echo "RPM_PACKAGE: ${RPM_PACKAGE}"
RPM_DATABASE="${GPHOME}/share/packages/database"
# on localhost
if [ 0 -eq $SKIP_LOCALHOST ]; then
    echo "Querying the MADlib RPM package installed on localhost..."
    rpm -q ${RPM_PACKAGE} --dbpath ${RPM_DATABASE}
    if [ $? = 0 ]; then
        echo "Attempting to remove RPM package (${RPM_PACKAGE}) before we can reinstall..."
        rpm -ve --allmatches ${RPM_PACKAGE} --dbpath ${RPM_DATABASE}
    fi
fi
# on all hosts
hosts=`cat ${HOSTFILE}`
for host in ${hosts}; do
    echo "Querying the MADlib RPM package on ${host}..."
    ssh ${host} rpm -q ${RPM_PACKAGE} --dbpath ${RPM_DATABASE}
    if [ $? = 0 ]; then
        echo "Attempting to remove RPM package (${RPM_PACKAGE}) on ${host} before we can reinstall..."
        ssh ${host} rpm -ve --allmatches ${RPM_PACKAGE} --dbpath ${RPM_DATABASE}
    fi
done

## ########################################################################
## installing the provided RPM
# local
if [ 0 -eq $SKIP_LOCALHOST ]; then
    rpm -v --nodeps --install ${RPM_FILEPATH} --dbpath ${RPM_DATABASE} \
        --prefix ${MADLIB_INSTALL_PATH}
    if [ 0 -ne $? ]; then
        echo "$0: Cannot successfully install ${RPM_FILEPATH} on localhost!"
        exit 1
    fi
fi
# remote
gpscp -v -f ${HOSTFILE} ${RPM_FILEPATH} =:${GPHOME}
gpssh -f ${HOSTFILE} rpm -v --nodeps --install ${GPHOME}/${RPM} \
    --dbpath ${RPM_DATABASE} --prefix ${MADLIB_INSTALL_PATH}
if [ 0 -ne $? ]; then
    echo "$0: Cannot successfully install ${RPM_FILEPATH} on one or more segment nodes!"
    exit 1
fi

echo "MADlib successfully installed."
echo "Please run the following command to deploy MADlib"
echo "usage:  madpack install -p hawq -c user@host:port/database"
echo "Example:"
echo "        \$ \${GPHOME}/madlib/bin/madpack install -p hawq -c gpadmin@mdw:5432/testdb"
echo "        This will install MADlib objects into a Greenplum database named \"testdb\""
echo "        running on server \"mdw\" on port 5432. Installer will try to login as \"gpadmin\""
echo "        and will prompt for password. The target schema will be \"madlib\"."
echo "For additional options run: madpack --help"
echo "Release notes and additional documentation can be found at http://madlib.apache.org/"

