/*
 * RESEARCH-ONLY referee Game Script for tools/ai-battle (see research/ai-battle/README.md).
 *
 * Observes only: it never issues a command, so it cannot affect the outcome of a match.
 * It prints one machine-readable "REF" line per company per economy quarter, which
 * tools/ai-battle/battle.py parses from the `-d script=1` log.
 */
class BattleReferee extends GSInfo {
	function GetAuthor()      { return "korczis/OpenTTD research fork"; }
	function GetName()        { return "BattleReferee"; }
	function GetShortName()   { return "BREF"; }
	function GetDescription() { return "AI-battle referee: logs company standings every quarter; read-only."; }
	function GetVersion()     { return 3; }
	function GetAPIVersion()  { return "16"; }
	function GetDate()        { return "2026-11-19"; }
	function CreateInstance() { return "BattleReferee"; }
}

RegisterGS(BattleReferee());
