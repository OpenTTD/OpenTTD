/* $Id$ */

class PriorityQueue extends AILibrary {
	function GetAuthor()      { return "OpenTTD NoAI Developers Team"; }
	function GetName()        { return "Priority Queue"; }
	function GetShortName()   { return "QUPQ"; }
	function GetDescription() { return "An implementation of a Priority Queue"; }
	function GetVersion()     { return 2; }
	function GetDate()        { return "2008-06-10"; }
	function CreateInstance() { return "PriorityQueue"; }
	function GetCategory()    { return "Queue"; }
}

RegisterLibrary(PriorityQueue());
