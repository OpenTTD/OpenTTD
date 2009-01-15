/* $Id$ */

class BinaryHeap extends AILibrary {
	function GetAuthor()      { return "OpenTTD NoAI Developers Team"; }
	function GetName()        { return "Binary Heap"; }
	function GetShortName()   { return "QUBH"; }
	function GetDescription() { return "An implementation of a Binary Heap"; }
	function GetVersion()     { return 1; }
	function GetDate()        { return "2008-06-10"; }
	function CreateInstance() { return "BinaryHeap"; }
	function GetCategory()    { return "Queue"; }
}

RegisterLibrary(BinaryHeap());
