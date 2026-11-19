/*
 * RESEARCH-ONLY referee Game Script -- see info.nut.
 *
 * Line format (one per company, all fields space separated key=value, name last because
 * it may contain spaces):
 *   REF Q date=<y>-<m> cid=<n> value=<v> money=<m> income=<i> perf=<p> vehicles=<n>
 *       stations=<n> name=<company name>
 * value/income/perf are for the last *completed* quarter (GSCompany quarter index 1);
 * money, vehicles and stations are the current values.
 */
class BattleReferee extends GSController {
	function Start();
	function Report();
}

function BattleReferee::Report()
{
	local d = GSDate.GetCurrentDate();
	for (local c = GSCompany.COMPANY_FIRST; c <= GSCompany.COMPANY_LAST; c++) {
		if (GSCompany.ResolveCompanyID(c) == GSCompany.COMPANY_INVALID) continue;
		local vl = GSVehicleList();
		vl.Valuate(GSVehicle.GetOwner);
		vl.KeepValue(c);
		local st = GSStationList(GSStation.STATION_ANY);
		st.Valuate(GSStation.GetOwner);
		st.KeepValue(c);
		/* GSLog.Error so the line shows at `-d script=1` without the AIs' info chatter. */
		GSLog.Error("REF Q date=" + GSDate.GetYear(d) + "-" + GSDate.GetMonth(d)
			+ " cid=" + c
			+ " value=" + GSCompany.GetQuarterlyCompanyValue(c, 1)
			+ " money=" + GSCompany.GetBankBalance(c)
			+ " income=" + GSCompany.GetQuarterlyIncome(c, 1)
			+ " perf=" + GSCompany.GetQuarterlyPerformanceRating(c, 1)
			+ " vehicles=" + vl.Count()
			+ " stations=" + st.Count()
			+ " name=" + GSCompany.GetName(c));
	}
}

function BattleReferee::Start()
{
	local last_q = -1;
	while (true) {
		local d = GSDate.GetCurrentDate();
		local q = GSDate.GetYear(d) * 4 + (GSDate.GetMonth(d) - 1) / 3;
		/* A few days into each quarter, so the previous quarter's statistics are settled. */
		if (q != last_q && GSDate.GetDayOfMonth(d) >= 3) {
			last_q = q;
			this.Report();
		}
		this.Sleep(74);
	}
}
