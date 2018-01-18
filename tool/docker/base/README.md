Docker Images
=======================

## Following docker configs are based on Debian (jessie) images:

1) Dockerfile_postgres_9_6: Plese use this configuration for development purposes. Please refer to the [top level README](https://github.com/apache/madlib/blob/master/README.md) for setup instructions.

2) Dockerfile_postgres_9_6_Jenkins: This configuration is used by the Jenkins builds (https://builds.apache.org/job/madlib-master-build and https://builds.apache.org/job/madlib-pr-build).

## The rest of the configs are based on CentOS images:

1) Dockerfile_centos_7_postgres_9.6 : Used for automated testing on centos 7 with postgres 9.6.
2) Dockerfile_centos_7_postgres_10 : Used for automated testing on centos 7 with postgres 10.

3) Dockerfile_centos_5_gpdb_4_3_10: This is work in progress, does not work at the moment.
