Since you are reading this, OpenTTD have crashed. This file tells you how 
to fix the most common problems or make to make a bug report, that the 
developers can use to track down the problem

The first thing you need to do is to get the error message. You can access OSX's build-in log by double-clicking Crash_Log_Opener. OTTD will do that if that file is present in the same folder as OTTD and is not renamed. However, major crashes can prevent the autoopen feature and you have to do it manually then 
If Crash_Log_Opener doesn't work you can view the log by opening Console inside Applications/utilities.

If you use the Console app, you should look at the buttom of the console.log window

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
