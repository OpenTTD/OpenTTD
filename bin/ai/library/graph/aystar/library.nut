/* $Id$ */

class AyStar extends AILibrary {
	function GetAuthor()      { return "OpenTTD NoAI Developers Team"; }
	function GetName()        { return "AyStar"; }
	function GetDescription() { return "An implementation of AyStar"; }
	function GetVersion()     { return 4; }
	function GetDate()        { return "2008-06-11"; }
	function CreateInstance() { return "AyStar"; }
}

RegisterLibrary(AyStar());
