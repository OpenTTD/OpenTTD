Compiling OpenTTD using Microsoft Visual C++ 6.0


---Step 1

Download the following files:

    * Useful.zip (http://sourceforge.net/project/showfiles.php?group_id=103924&package_id=114307&release_id=228633)
    * SDL-1.2.8-VC6.zip (http://www.libsdl.org/release/SDL-devel-1.2.8-VC6.zip)
    * DirectX7.0 SDK (http://www.tt-forums.net/download.php?id=15989) (or alternatively the latest DirectX SDK from Microsoft)
    * afxres.h (http://www-d0.fnal.gov/d0dist/dist/packages/d0ve/devel/windows/AFXRES.H) 

...and of course the newest source from svn://svn.openttd.com/trunk

(The alpha version of the new map array can be found at svn://svn.openttd.com/branch/map)

You have to have and SVN-client to download the source:

    * Command line version (http://subversion.tigris.org/servlets/ProjectDocumentList?folderID=91)
    * TortoiseSVN (http://tortoisesvn.tigris.org/download.html) 


---Step 2

Put the newly downloaded files in the VC lib and include directories (Where C:\program files\ is your local location of VC)


    * zconf.h [useful.zip]
    * zlib.h [useful.zip]
    * png.h [useful.zip]
    * pngconf.h [useful.zip]
    * afxres.h 

in

  C:\Program Files\Microsoft Visual Studio\VC98\Include

and

    * zlibstat.lib [useful.zip]
    * SDL.lib [SDL.zip]
    * libpng.lib [useful.zip] 

in

  C:\Program Files\Microsoft Visual Studio\VC98\Lib


---Step 3: DirectX SDK

(This should work with the latest DirectX SDK as well.) The installation with DirectX 7 was odd, so you'd better use the version available via the forum, see also the download link on top.

There are 2 folder in the compressed file: Include and Lib

Copy all files from Include folder to

C:\Program Files\Microsoft Visual Studio\VC98\Include

and all files from Lib folder to

C:\Program Files\Microsoft Visual Studio\VC98\Lib



You can also make custom directories, for libraries (.lib) and includes/header files (.h) and add it to the VC paths via:

Tools -> Options -> Directories -> show directories for:

a) include files (the include dir: C:\Program Files\Microsoft Visual Studio\VC98\DirectX 7 SDK\include )

b) library files (the lib dir, C:\Program Files\Microsoft Visual Studio\VC98\DirectX 7 SDK\lib )


---Step 4

Copy the following files from Transport Tycoon Deluxe to the data folder

    * sample.cat
    * trg1r.grf
    * trgcr.grf
    * trghr.grf
    * trgir.grf
    * trgtr.grf 


---Step 5

Open trunk/ttd.dsw

Build menu > Set active configuration > Select: "ttd - Win32 Release with PNG"

Compile...


Now it should work, it worked for me :)

From r1319 you can compile branch/map in Debug mode (by Bociusz)

If it's not working, and you checked that you using the newest SVN (!) report to Bociusz on IRC (irc://irc.freenode.net/#openttd)

Go ahead and make that patch! Happy Hacking! :)

Originally written by Dribbel

Project file updating by Bociusz