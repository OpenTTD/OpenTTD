class StationList extends AIInfo {
	function GetAuthor()      { return "OpenTTD NoAI Developers Team"; }
	function GetName()        { return "StationList"; }
	function GetShortName()   { return "REGS"; }
	function GetDescription() { return "This runs stationlist-tests on some commands. On the same map the result should always be the same."; }
	function GetVersion()     { return 1; }
	function GetAPIVersion()  { return "1.12"; }
	function GetDate()        { return "2007-03-18"; }
	function CreateInstance() { return "StationList"; }
}

RegisterAI(StationList());

