/* $Id$ */

struct CFsaItem
{
	int i;

	FORCEINLINE static int& NumInstances() { static int num_instances = 0; return num_instances; };

	FORCEINLINE CFsaItem(int i = 0)
	{
		this->i = i;
		NumInstances()++;
		DBG("(*)");
	}

	FORCEINLINE CFsaItem(const CFsaItem& src)
	{
		this->i = src.i;
		NumInstances()++;
		DBG("(c)");
	}

	FORCEINLINE ~CFsaItem()
	{
		NumInstances()--;
		DBG("(-)");
	}
};

typedef CFixedSizeArrayT<CFsaItem, 4> CSubArray;
typedef CFixedSizeArrayT<CSubArray, 4> CSuperArray;

static int TestFixedSizeArray(bool silent)
{
	int res = 0;
	{
		CSuperArray a;

		CHECK_INT(0, a.IsFull(), false);
		CHECK_INT(1, a.IsEmpty(), true);

		CSubArray& b1 = a.Add();
		b1.Add().i = 1;
		new(&b1.AddNC())CFsaItem(2);

		CSubArray& b2 = a.Add();
		new(&b2.AddNC())CFsaItem(3);
		b2.Add().i = 4;

		CSubArray& b3 = a.AddNC();
		new(&b3)CSubArray(b1);

		CSubArray& b4 = a.AddNC();
		new(&b4)CSubArray(b2);

		CHECK_INT(2, a[0][0].i, 1);
		CHECK_INT(3, b1[1].i, 2);
		CHECK_INT(4, b1.Size(), 2);
		CHECK_INT(5, a[3][0].i, 3);
		CHECK_INT(6, a[3][1].i, 4);
		CHECK_INT(7, CFsaItem::NumInstances(), 4);
		CHECK_INT(8, a.IsFull(), true);
		CHECK_INT(9, a.IsEmpty(), false);
		CHECK_INT(10, a[3].IsFull(), false);
		CHECK_INT(11, a[3].IsEmpty(), false);
	}
	CHECK_INT(12, CFsaItem::NumInstances(), 0);

	return res;
}

typedef CArrayT<CFsaItem, 2> CArray;

static int TestArray(bool silent)
{
	int res = 0;
	{
		CArray a;

		CHECK_INT(0, a.IsFull(), false);
		CHECK_INT(1, a.IsEmpty(), true);

		CHECK_INT(2, a.Size(), 0);

		a.Add().i = 1;
		CHECK_INT(3, a.Size(), 1);

		new(&a.AddNC())CFsaItem(2);
		CHECK_INT(4, a.Size(), 2);

		CHECK_INT(5, a.IsFull(), false);
		CHECK_INT(6, a.IsEmpty(), false);

		a.Add().i = 3;
		CHECK_INT(7, a.Size(), 3);

		new(&a.AddNC())CFsaItem(4);
		CHECK_INT(8, a.Size(), 4);

		CHECK_INT(9, a[0].i, 1);
		CHECK_INT(10, a[1].i, 2);
		CHECK_INT(11, a[2].i, 3);
		CHECK_INT(12, a[3].i, 4);

		CHECK_INT(13, a.IsFull(), true);
		CHECK_INT(14, a.IsEmpty(), false);

		CHECK_INT(15, CFsaItem::NumInstances(), 4);
	}
	CHECK_INT(16, CFsaItem::NumInstances(), 0);

	return res;
}
