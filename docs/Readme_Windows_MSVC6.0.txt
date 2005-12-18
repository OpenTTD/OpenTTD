Compiling OpenTTD using Microsoft Visual C++ 6.0


Step 1: Ingredients

Download the following files:

    * Openttd-useful.zip (http://sourceforge.net/project/showfiles.php?group_id=103924&amp;package_id=114307&amp;release_id=228633)
    * DirectX 8.1 SDK (http://neuron.tuke.sk/~mizanin/eng/Dx81sdk-include-lib.rar) (or alternatively the latest DirectX SDK from Microsoft)
    * The February 2003 Microsoft Platform SDK (http://www.microsoft.com/msdownload/platformsdk/sdkupdate/XPSP2FULLInstall.htm) (newer SDK's do not work with MSVC6)
    * afxres.h (http://www-d0.fnal.gov/d0dist/dist/packages/d0ve/devel/windows/AFXRES.H) (maybe you not need this)

...and of course the newest source from svn://svn.openttd.org/trunk

You have to have a SVN-client to download the source:

    * Command line version (Subversion 1.2.3 Win32 binaries) (http://subversion.tigris.org/servlets/ProjectDocumentList?folderID=91)
    * GUI TortoiseSVN (http://tortoisesvn.tigris.org/download.html)

Step 2: Includes and Libraries

Put the newly downloaded files in the VC lib and include directories (Where "C:\Program Files\Microsoft Visual Studio\VC98" is your local location of VC)


    * zconf.h [useful.zip]
    * zlib.h [useful.zip]
    * png.h [useful.zip]
    * pngconf.h [useful.zip]
    * afxres.h

in

 C:\Program Files\Microsoft Visual Studio\VC98\Include

and

    * zlibstat.lib [useful.zip]
    * libpng.lib [useful.zip]

in

 C:\Program Files\Microsoft Visual Studio\VC98\Lib


Step 3: DirectX SDK

(This should work with the latest DirectX SDK as well.)

There are 2 folder in the compressed file: Include and Lib

Copy all files from Include folder to

C:\Program Files\Microsoft Visual Studio\VC98\Include

and all files from Lib folder to

C:\Program Files\Microsoft Visual Studio\VC98\Lib



You can also make custom directories, which is recommended so you don't overwrite VS6 files, for libraries (.lib) and includes/header files (.h) and add it to the VC paths via:

Tools -> Options -> Directories -> show directories for:

a) include files (the include dir: C:\Program Files\Microsoft Visual Studio\VC98\DirectX 8.1 SDK\include )

b) library files (the lib dir, C:\Program Files\Microsoft Visual Studio\VC98\DirectX 8.1 SDK\lib )

NOTE: make sure that the directory for the DirectX SDK is the first one in the list, above all others, otherwise compilation will most likely fail!!

Step 4: TTD Graphics files

Copy the following files from Transport Tycoon Deluxe to the data folder

    * sample.cat
    * trg1r.grf
    * trgcr.grf
    * trghr.grf
    * trgir.grf
    * trgtr.grf


Step 5: Compiling

Open trunk/openttd.dsw

Build menu > Set active configuration > Select: "openttd - Win32 Release"

Compile...


Now it should work, it worked for me :)

From r1319 you can compile branch/map in Debug mode (by Bociusz)

For compiling branch/cargo-packets you have to add cargo.c and .h to this tree's openttd.dsp

If it's not working, and you checked that you using the newest SVN (!) report to Bociusz on IRC (irc://irc.freenode.net/openttd)

Go ahead and make that patch! Happy Hacking! :)

Originally written by Dribbel

Project file updating by Bociusz
