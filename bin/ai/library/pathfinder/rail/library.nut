/* $Id$ */

class Rail extends AILibrary {
	function GetAuthor()      { return "OpenTTD NoAI Developers Team"; }
	function GetName()        { return "Rail"; }
	function GetDescription() { return "An implementation of a rail pathfinder"; }
	function GetVersion()     { return 1; }
	function GetDate()        { return "2008-09-22"; }
	function CreateInstance() { return "Rail"; }
}

RegisterLibrary(Rail());
