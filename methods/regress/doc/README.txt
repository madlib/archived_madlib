Multilinear Regression
======================

Installation instructions
-------------------------

Requirements:
- An installed LAPACK library. Many OSs ship with this library preinstalled (e.g.,
  Mac OS X includes the vecLib framework, which includes LAPACK).

To compile and link use the makefile.


Included third-party soures
---------------------------

We include parts of the CEPHES library to compute the Student-t distribution
function (stdtr).

Note the following issues with CEPHES:
- calls lgamma, which is not thread safe (global variable signgam will be
  changed)
- contains platform-specific definitions in mconf.h (see also const.c)
- License is not very explicit: All that is available is the following read-me
  on http://www.netlib.org/cephes/readme
  
  "
	   Some software in this archive may be from the book _Methods and
	Programs for Mathematical Functions_ (Prentice-Hall or Simon & Schuster
	International, 1989) or from the Cephes Mathematical Library, a
	commercial product. In either event, it is copyrighted by the author.
	What you see here may be used freely but it comes with no support or
	guarantee.

	   The two known misprints in the book are repaired here in the
	source listings for the gamma function and the incomplete beta
	integral.


	   Stephen L. Moshier
	   moshier@na-net.ornl.gov
	"

- The author has been mailed about explicit permissions, but no response so far.


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
