Since you are reading this, OpenTTD have crashed. This file tells you how 
to fix the most common problems or make to make a bug report, that the 
developers can use to track down the problem

If it is an assert, OpenTTD will open the console for you, if it is truly a crash, you have to do it yourself. The Console is located at /Applications/Utilities/Console.
The problem is near the button of the page

The problems are as follows:
NOTE: build from source means to download the source and compile 
yourself. If you get one of the build from source error with the version 
that is downloaded on a dmg file, you should make a bug report

--Didn't find a needed file:
	you just give it the file it asks for. It even tells you what 
folder it wants it in
	most common version of this problem is "Error: Cannot open file 
'data/sample.cat'"
	if you get that one, that means that you haven't got all the 
needed files from the WINDOWS version of TTD
	or if you build from source, 

--Error: No available language packs
	you need at least one .lng file in your lang folder. This applies 
only to people who build from source

--spritecache.c:237: failed assertion `b'
	you got an outdated grf file. Update from the data folder in the 
source. This applies only to people, who build from 	source

--assertion error that are not triggered by one of the errors listed in 
this file:
	you most likely found a bug. Write down the assertion and try to 
see if you can reproduce it. If you can, make a
	savegame from just before it happens (autosaves are useful here) 
and post a bugreport with it on sourceforge
	Write what you did to trigger the bug and what assertion it made
