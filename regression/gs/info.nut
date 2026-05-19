class Regression extends GSInfo {
	function GetAuthor()      { return "OpenTTD GS Developers Team"; }
	function GetName()        { return "Regression"; }
	function GetShortName()   { return "REGR"; }
	function GetDescription() { return "This runs regression-tests on some commands. On the same map the result should always be the same."; }
	function GetVersion()     { return 1; }
	function GetAPIVersion()  { return "16"; }
	function GetDate()        { return "2026-05-19"; }
	function CreateInstance() { return "Regression"; }
	function UseAsRandomAI()  { return false; }
}

RegisterGS(Regression());

