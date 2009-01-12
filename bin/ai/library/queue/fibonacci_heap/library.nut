/* $Id$ */

class FibonacciHeap extends AILibrary {
	function GetAuthor()      { return "OpenTTD NoAI Developers Team"; }
	function GetName()        { return "Fibonacci Heap"; }
	function GetDescription() { return "An implementation of a Fibonacci Heap"; }
	function GetVersion()     { return 1; }
	function GetDate()        { return "2008-08-22"; }
	function CreateInstance() { return "FibonacciHeap"; }
}

RegisterLibrary(FibonacciHeap());
