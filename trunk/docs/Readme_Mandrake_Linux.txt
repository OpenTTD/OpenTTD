Compiling and developing OpenTTD on MandrakeLinux 10.0 Official

A quick guide to get started with OpenTTD development on Linux.
---------------------------------------------------------------

1.) RPMs:

Most packages that are required for development (like gcc) should already be installed on your box. You will require those RPMs additionally:
- libsdl1.2-devel-1.2.7-2mdk
- subversion-1.0.1-1mdk (+ dependencies)
- libsvn_ra_svn1_0-1.0.1-1mdk


2.) Subversion:

To obtain the source code from the subversion server type
  $ svn co svn://svn.openttd.com/openttd/trunk openttd
from command line to dump the code into the directory 'openttd'.

To update your working copy to the latest revision use
  $ svn update
Don't worry, your version will be merged with the latest version.

The command
  $ svn diff > mypatch.diff
creates a patch file (aka diff file) which you can submit to the developers to share your improvements.

You can undo changes to a file with
  $svn revert filename


3.) Required data files:

Copy the following files from the WINDOWS version of Transport Tycoon Deluxe to openttd/data/
  sample.cat
  trg1r.grf
  trgcr.grf
  trghr.grf
  trgir.grf
  trgtr.grf
  
  
4.) Compiling and running:

Compile OpenTTD with
  $ make
and run it with
  $ ./ttd

     
5.) Playing the soundtrack:

If you want the original TTD music you need to copy the whole /gm/ directory from Windows. Additionally the TiMidity driver is required:
  TiMidity++-2.12.0-0.pre1.4mdk (+ dependencies)
To run OpenTTD with music support type
  $ ./ttd -m extmidi

  
X.) Last Update: $Date: 2004-06-01 19:08:09 +0200 (Tue, 01 Jun 2004) $
    Written for revision $Rev: 710 $