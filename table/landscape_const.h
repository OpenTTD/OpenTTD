static const LandscapePredefVar _landscape_predef_var[4] = {
	{
		/* normal names */
		{
			STR_000F_PASSENGERS,
			STR_0010_COAL,
			STR_0011_MAIL,
			STR_0012_OIL,
			STR_0013_LIVESTOCK,
			STR_0014_GOODS,
			STR_0015_GRAIN,
			STR_0016_WOOD,
			STR_0017_IRON_ORE,
			STR_0018_STEEL,
			STR_0019_VALUABLES,
			STR_000E,
		},
		/* normal weights */
		{
			1, 16, 4, 16, 3, 8, 16, 16, 16, 16, 2, 0,
		},

		/* normal sprites */
		{
			0x10C9, 0x10CA, 0x10CB, 0x10CC, 0x10CD, 0x10CE, 0x10CF, 0x10D0, 0x10D1, 0x10D2, 0x10D3,
		},

		/* normal initial cargo payment */
		{
			3185, 5916, 4550, 4437, 4322, 6144, 4778, 5005, 5120, 5688, 7509, 5688
		},

		/* normal transit days table 1 */
		{
			0, 7, 20, 25, 4, 5, 4, 15, 9, 7, 1, 0,
		},

		/* normal transit days table 2 */
		{
			24, 255, 90, 255, 18, 28, 40, 255, 255, 255, 32, 30,
		},

		/* normal railveh by cargo */
		{
			{27, 29, 28, 30, 31, 32, 33, 34, 35, 36, 37, 38},
			{57, 59, 58, 60, 61, 62, 63, 64, 65, 66, 67, 68},
			{89, 91, 90, 92, 93, 94, 95, 96, 97, 98, 99, 100}
		},

		/* normal road veh by cargo start & count */
		{116, 123, 126, 132, 135, 138, 141, 144, 147, 150, 153, 156},
		{7, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3}
	},

	{
		/* hilly names */
		{
			STR_000F_PASSENGERS,
			STR_0010_COAL,
			STR_0011_MAIL,
			STR_0012_OIL,
			STR_0013_LIVESTOCK,
			STR_0014_GOODS,
			STR_0022_WHEAT,
			STR_0016_WOOD,
			STR_000E,
			STR_001F_PAPER,
			STR_0020_GOLD,
			STR_001E_FOOD,			
		},
		/* hilly weights */
		{
			1, 16, 4, 16, 3, 8, 16, 16, 0, 16, 8, 16
		},

		/* hilly sprites */
		{
			0x10C9, 0x10CA, 0x10CB, 0x10CC, 0x10CD, 0x10CE, 0x10CF, 0x10D0, 0x2, 0x10D9, 0x10D3, 0x10D8
		},

		/* hilly initial cargo payment */
		{
			3185, 5916, 4550, 4437, 4322, 6144, 4778, 5005, 5120, 5461, 5802, 5688
		},

		/* hilly transit days table 1 */
		{
			0, 7, 20, 25, 4, 5, 4, 15, 9, 7, 10, 0,
		},

		/* hilly transit days table 2 */
		{
			24, 255, 90, 255, 18, 28, 40, 255, 255, 60, 40, 30
		},

		/* hilly railveh by cargo */
		{
			{27, 29, 28, 30, 31, 32, 33, 34, 35, 39, 37, 38},
			{57, 59, 58, 60, 61, 62, 63, 64, 65, 69, 67, 68},
			{89, 91, 90, 92, 93, 94, 95, 96, 97, 101, 99, 100}
		},


		/* hilly road veh by cargo start & count */
		{116, 123, 126, 132, 135, 138, 141, 144, 147, 159, 153, 156},
		{7, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3},
	},


	{
		/* desert names */
		{
			STR_000F_PASSENGERS,
			STR_0023_RUBBER,
			STR_0011_MAIL,
			STR_0012_OIL,
			STR_001C_FRUIT,
			STR_0014_GOODS,
			STR_001B_MAIZE,
			STR_0016_WOOD,
			STR_001A_COPPER_ORE,
			STR_0021_WATER,
			STR_001D_DIAMONDS,
			STR_001E_FOOD
		},
		/* desert weights */
		{
			1, 16, 4, 16, 16, 8, 16, 16, 16, 16, 2, 16,
		},

		/* desert sprites */
		{
			0x10C9, 0x10DA, 0x10CB, 0x10CC, 0x10D4, 0x10CE, 0x10CF, 0x10D0, 0x10D5, 0x10D6, 0x10D7, 0x10D8
		},

		/* desert initial cargo payment */
		{
			3185, 4437, 4550, 4892, 4209, 6144, 4322, 7964, 4892, 4664, 5802, 5688
		},

		/* desert transit days table 1 */
		{
			0, 2, 20, 25, 0, 5, 4, 15, 12, 20, 10, 0
		},

		/* desert transit days table 2 */
		{
			24, 20, 90, 255, 15, 28, 40, 255, 255, 80, 255, 30
		},

		/* desert railveh by cargo */
		{
			{27, 43, 28, 30, 42, 32, 33, 34, 40, 41, 37, 38},
			{57, 73, 58, 60, 72, 62, 63, 64, 70, 71, 67, 68},
			{89, 105, 90, 92, 104, 94, 95, 96, 102, 103, 99, 100}
		},

		/* desert road veh by cargo start & count */
		{116, 171, 126, 132, 168, 138, 141, 144, 162, 165, 153, 156},
		{7, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3}
	},

	{
		/* candy names */
		{
			STR_000F_PASSENGERS,
			STR_0024_SUGAR,
			STR_0011_MAIL,
			STR_0025_TOYS,
			STR_002B_BATTERIES,
			STR_0026_CANDY,
			STR_002A_TOFFEE,
			STR_0027_COLA,
			STR_0028_COTTON_CANDY,
			STR_0029_BUBBLES,
			STR_002C_PLASTIC,
			STR_002D_FIZZY_DRINKS,
		},
		/* candy weights */
		{
			1, 16, 4, 2, 4, 5, 16, 16, 16, 1, 16, 2
		},

		/* candy sprites */
		{
			0x10C9, 0x10DC, 0x10CB, 0x10DD, 0x10E3, 0x10DB, 0x10E0, 0x10D6, 0x10DE, 0x10E1, 0x10E2, 0x10DF
		},

		/* candy initial cargo payment */
		{
			3185, 4437, 4550, 5574, 4322, 6144, 4778, 4892, 5005, 5077, 4664, 6250
		},

		/* candy transit days table 1 */
		{
			0, 20, 20, 25, 2, 8, 14, 5, 10, 20, 30, 30,
		},

		/* candy transit days table 2 */
		{
			24, 255, 90, 255, 30, 40, 60, 75, 25, 80, 255, 50
		},

		/* candy railveh by cargo */
		{
			{27, 44, 28, 50, 51, 49, 46, 48, 45, 47, 53, 52},
			{57, 74, 58, 80, 81, 79, 76, 78, 75, 77, 83, 82},
			{89, 106, 90, 112, 113, 111, 108, 110, 107, 109, 115, 114}
		},

		/* candy road veh by cargo start & count */
		{116, 174, 126, 186, 192, 189, 183, 177, 180, 201, 198, 195},
		{7, 3, 6, 3, 3, 3, 3, 3, 3, 3, 3, 3}
	}
};
