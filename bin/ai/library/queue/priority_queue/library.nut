/* $Id$ */

class Priority_Queue extends AILibrary {
	function GetAuthor()      { return "OpenTTD NoAI Developers Team"; }
	function GetName()        { return "Priority Queue"; }
	function GetShortName()   { return "QUPQ"; }
	function GetDescription() { return "An implementation of a Priority Queue"; }
	function GetVersion()     { return 2; }
	function GetDate()        { return "2008-06-10"; }
	function CreateInstance() { return "Priority_Queue"; }
	function GetCategory()    { return "Queue"; }
}

RegisterLibrary(Priority_Queue());
