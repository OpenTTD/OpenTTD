Compiling OpenTTD using Microsoft Visual C++
January 2, 2007
--------------------------------------------
PLEASE READ THE ENTIRE DOCUMENT BEFORE DOING ANY ACTUAL CHANGES!!


SUPPORTED MSVC COMPILERS
------------------------
OpenTTD includes projects for MSVC 2003.NET and MSVC 2005.NET. Both will
compile out of the box, providing you have the required libraries/headers;
which ones, see below. There is no support for VS6, you are therefore
strongly encouraged to either upgrade to MSVC 2005 Express (free) or use GCC.
MSVC 2002 probably works as well, but it has not been tested.


1) REQUIRED FILES
-----------------
You might already have some of the files already installed, so check before
downloading; mostly because the DirectX SDK and Platform SDK are about
500MB each.
Download the following files:

	* openttd-useful.zip (http://sf.net/project/showfiles.php?group_id=103924&package_id=114307)
	* DirectX 8.1 SDK (http://neuron.tuke.sk/~mizanin/eng/Dx81sdk-include-lib.rar) (or alternatively the latest DirectX SDK from Microsoft)
	* MS Windows Platform SDK (http://www.microsoft.com/downloads/details.aspx?FamilyId=A55B6B43-E24F-4EA3-A93E-40C0EC4F68E5&displaylang=en)
	* afxres.h (http://www-d0.fnal.gov/d0dist/dist/packages/d0ve/devel/windows/AFXRES.H)

...and of course the newest source from svn://svn.openttd.org/trunk

You need an SVN-client to download the source from subversion:

	* CLI Subversion (http://subversion.tigris.org/servlets/ProjectDocumentList?folderID=91)
	* GUI TortoiseSVN (http://tortoisesvn.tigris.org/download.html)


2) INCLUDES AND LIBRARIES
-------------------------
Put the newly downloaded files in the VC lib\ and include\ directories; where
"C:\Program Files\Microsoft Visual Studio 8\VC" is your location of Visual C.
If you are compiling for an x64 system, use the include\ and lib\ directories
from the win64/ folder.

	* openttd-useful.zip\include\*
	* afxresh.h
	to >	C:\Program Files\Microsoft Visual Studio 8\VC\Include

	* openttd-useful.zip\lib\*
	to >	C:\Program Files\Microsoft Visual Studio 8\VC\Lib

Custom directories might be recommended, check 2.2)


2.1) INCLUDES AND LIBRARIES - DIRECTX/PLATFORM SDK
--------------------------------------------------
Basically the same procedure as with the useful zip file, providing
you are not using the Microsoft installer. Put the include files in the
include\ directory and the library files to the Lib\ directory.

It is recommended to use custom directories so you don't overwrite any
default header or library files.


2.2) CUSTOM DIRECTORIES
-----------------------
If you have put the above include and/or library files into custom folders,
MSVC will not find them by default. You need to add these paths to VC through:

Tools > Options > Projects and Solutions > VC++ Directories > show directories for

	* Include files: Add the DirectX/Platform SDK include dir you've created
	* Library files: Add the path to the SDK custom lib dir

NOTE: make sure that the directory for the DirectX SDK is the first one in the
list, above all others, otherwise compilation will most likely fail!!


2.3) DEBUGGING - WORKING DIRECTORY (MSVC 2003 ONLY!)
----------------------------------------------------
The very first time you check out and compile OpenTTD with Visual Studio 2003, running
the binary will complain about missing files. You need to go into and change a setting

OpenTTD > Project > Properties > Configuration (All Configurations) > ...
 Configuration Properties > Debugging >

	* Working Directory: ..\bin

VS 2005 works out of the box because Microsoft allowed a user to supply a humanly-
readable defaults file (openttd_vs80.vcproj.user), whereas 2003 is braindead.


3) TTD GRAPHICS FILES
---------------------
Copy the following files from Transport Tycoon Deluxe to the bin/data folder

	* sample.cat
	* trg1r.grf
	* trgcr.grf
	* trghr.grf
	* trgir.grf
	* trgtr.grf


4) COMPILING
------------
Open trunk/openttd[_vs80].sln
Set the build mode to 'Release' in
Build > Configuration manager > Active solution configuration > select "Release"
Compile...

If everything works well the binary should be in trunk/objs/[Win32]/Release/openttd.exe


5) EDITING, CHANGING SOURCE CODE
--------------------------------
Set the build mode (back to) 'Debug'
Change the startup project to openttd by right-clicking the 'openttd' project
in the Solution Explorer and selecting 'Set as Startup Project'. The 'openttd'
project should now show up bold instead of 'strgen'.


6) PROBLEMS?
------------
If compilation fails, double-check that you are using the latest SVN (!)
source. If it still doesn't work, check in on IRC (irc://irc.oftc.net/openttd),
to ask about reasons; or just wait. The problem will most likely solve itself
within a few days as the problem is noticed and fixed.

An up-to-date version of this README can be found on the wiki:
http://wiki.openttd.org/index.php/MicrosoftVisualCExpress