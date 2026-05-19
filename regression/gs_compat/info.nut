class CompatRegression extends GSInfo {
	function GetAuthor()      { return "OpenTTD GS Developers Team"; }
	function GetName()        { return "Compat Regression"; }
	function GetShortName()   { return "REGC"; }
	function GetDescription() { return "This runs regression on the compat scripts."; }
	function GetVersion()     { return 1; }
	function GetAPIVersion()  { return "1.2"; }
	function GetDate()        { return "2026-05-19"; }
	function CreateInstance() { return "CompatRegression"; }
	function UseAsRandomAI()  { return false; }
}

RegisterGS(CompatRegression());
