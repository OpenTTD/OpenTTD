class Regression extends GSController {
	function Start();
};



function Regression::TestInit()
{
	print("");
	print("--TestInit--");
	print(" Ops:      " + this.GetOpsTillSuspend());
	print(" TickTest: " + this.GetTick());
	this.Sleep(1);
	print(" TickTest: " + this.GetTick());
	print(" Ops:      " + this.GetOpsTillSuspend());
	print(" SetCommandDelay: " + GSController.SetCommandDelay(1));
	print(" IsValid(vehicle.plane_speed): " + GSGameSettings.IsValid("vehicle.plane_speed"));
	print(" vehicle.plane_speed: " + GSGameSettings.GetValue("vehicle.plane_speed"));
	require("require.nut");
	print(" TestEnum.value1: " + ::TestEnum.value1);
	print(" test_constant: " + ::test_constant);
	print(" TestEnum.value2: " + TestEnum.value2);
	print(" test_constant: " + test_constant);
	print(" min(6, 3): " + min(6, 3));
	print(" min(3, 6): " + min(3, 6));
	print(" max(6, 3): " + max(6, 3));
	print(" max(3, 6): " + max(3, 6));
}

function Regression::Company()
{
	print("");
	print("--Company--");

	/* Test GSXXXMode() in scopes */
	{
		local test = GSTestMode();
		print("  SetName():            " + GSCompany.SetName("Regression"));
		print("  SetName():            " + GSCompany.SetName("Regression"));
		{
			local exec = GSExecMode();
			print("  SetName():            " + GSCompany.SetName("Regression"));
			print("  SetName():            " + GSCompany.SetName("Regression"));
			print("  GetLastErrorString(): " + GSError.GetLastErrorString());
		}
	}

	print("  GetName():                         " + GSCompany.GetName(GSCompany.COMPANY_SELF));
	print("  GetPresidentName():                " + GSCompany.GetPresidentName(GSCompany.COMPANY_SELF));
	print("  SetPresidentName():                " + GSCompany.SetPresidentName("Regression GS"));
	print("  GetPresidentName():                " + GSCompany.GetPresidentName(GSCompany.COMPANY_SELF));

	print("");
	print("--CompanyMode--");

	local company = GSCompanyMode(GSCompany.COMPANY_FIRST);
	{
		local test = GSTestMode();
		print("  SetName():            " + GSCompany.SetName("Regression"));
		print("  SetName():            " + GSCompany.SetName("Regression"));
		{
			local exec = GSExecMode();
			print("  SetName():            " + GSCompany.SetName("Regression"));
			print("  SetName():            " + GSCompany.SetName("Regression"));
			print("  GetLastErrorString(): " + GSError.GetLastErrorString());
		}
	}

	print("  GetName():                         " + GSCompany.GetName(GSCompany.COMPANY_SELF));
	print("  GetPresidentName():                " + GSCompany.GetPresidentName(GSCompany.COMPANY_SELF));
	print("  SetPresidentName():                " + GSCompany.SetPresidentName("Regression GS"));
	print("  GetPresidentName():                " + GSCompany.GetPresidentName(GSCompany.COMPANY_SELF));
}

function Regression::Start()
{
	this.TestInit();
	this.Company();

	print("");
	print("Done...");
	print("");
}
