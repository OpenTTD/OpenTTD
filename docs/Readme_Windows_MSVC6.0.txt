Compilung OpenTTD using MS VC6.0


Step 1
------------------
Downloaded:
Useful.zip	http://sourceforge.net/project/showfiles.php?group_id=103924&package_id=114307&release_id=228633
SDL.zip		http://www.libsdl.org/release/SDL-1.2.7-win32.zip
DirectX7.0 SDK	http://www.tt-forums.net/download.php?id=15989
		(or alternatively the latest DirectX SDK from Microsoft)
afxres.h	http://www-d0.fnal.gov/d0dist/dist/packages/d0ve/devel/windows/AFXRES.H


Step 2
------------------
Put the newly downloaded files in the VC lib and include directories
(Where D:\program files\ is your local location of VC)


* zconf.h		[useful.zip]
* zlib.h		[useful.zip]
* afxres.h	
in
  D:\Program Files\Microsoft Visual Studio\VC98\Include

* zlibstat.lib		[usefull.zip]
* SDL.lib		[SDL.zip
* libpng.lib		[usefull.zip]
in
  D:\Program Files\Microsoft Visual Studio\VC98\Lib

You can also make custum directories, for libraries (.lib) and includes/header files (.h) and
add it to the VC paths via: 
Tools -> Options -> Directories -> show directories for:
a) include files (the include dir: D:\Program Files\Microsoft Visual Studio\VC98\DirectX 7 SDK\include )
b) library files (the lib dir, D:\Program Files\Microsoft Visual Studio\VC98\DirectX 7 SDK\lib )


Step 3: DirextX SDK
------------------
(This should work with the latest DirectX SDK as well.)
The installation with DirectX 7 was odd, so you'd better use the version available via the forum, see also
the download link on top.

Copy the DirectX 7 SDK files, leaving the directory stucture intact, to the directory:
  D:\Program Files\Microsoft Visual Studio\VC98\
thus resulting in
  D:\Program Files\Microsoft Visual Studio\VC98\DirectX 7 SDK\include and
  D:\Program Files\Microsoft Visual Studio\VC98\DirectX 7 SDK\lib

Step 3.1

Add these two folders to the search path of VC.
In VC6.0: Tools -> Options -> Directories -> show directories for:
a) include files (the include dir: D:\Program Files\Microsoft Visual Studio\VC98\DirectX 7 SDK\include )
b) libraru files (the lib dir, D:\Program Files\Microsoft Visual Studio\VC98\DirectX 7 SDK\lib )


Step 4
-----------------
Copy the following files from Transport Tycoon Deluxe to the data folder
  sample.cat
  trg1r.grf
  trgcr.grf
  trghr.grf
  trgir.grf
  trgtr.grf

(Alternatively you can use the TTD GRF files from the DOS version: TRG1.GRF, TRGC.GRF, TRGH.GRF, TRGI.GRF, TRGT.GRF. Those filenames have to be uppercase to be detected correctly. A few minor graphical glitches with the DOS graphics remain. E.g. the autorail button in the rail toolbar doesn't look as nice as with the Windows graphics.)

Step 5
-----------------
Compile ...


Step 6
-----------------

Now it should work, it worked for me :)

Go ahead and make that patch!

Happy Hacking!



------------------
written by Dribbel

