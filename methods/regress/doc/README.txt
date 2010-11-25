Multilinear Regression
======================

Installation instructions
-------------------------

Requirements:
- An installed LAPACK library. Many OSs ship with this library preinstalled (e.g.,
  Mac OS X includes the vecLib framework, which includes LAPACK).

To compile and link use the makefile.


XCode
-----

In opt/XCode, there is an XCode project, which is particularly useful for
debugging.

0. Modify the *.xcconfig files to contain the correct paths to
   Greenplum/PostgreSQL. When building with XCode, choose the correct configuration
   and architecture.
1. Do a "select pg_backend_pid();" in psql
2. Choose "Run" -> "Attach to Process" -> "Process ID..." in XCode and enter
   the process ID obtained in psql
