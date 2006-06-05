/* $Id$ */

struct CData
{
	int val;

	FORCEINLINE CData() : val(0)                       {NumInstances()++; /*DBG("DCata::ctor()\n");*/}
	FORCEINLINE CData(const CData& src) : val(src.val) {NumInstances()++; /*DBG("DCata::ctor(%d)\n", val);*/}
	FORCEINLINE ~CData()                               {NumInstances()--; /*DBG("DCata::dtor(%d)\n", val);*/}

	FORCEINLINE bool operator < (const CData& other) const {return (val < other.val);}

	FORCEINLINE static int& NumInstances() { static int num_instances = 0; return num_instances; };

};

typedef CAutoCopyPtrT<CData> PData;

static int TestAutoCopyPtr(bool silent)
{
	int res = 0;
	{
		PData p1, p3;
		p1->val = 4;
		PData p2; p2 = p1;
		p2->val = 6;
		DBG("\n%d, %d", p1->val, p2->val);
		CHECK_INT(0, p1->val, 4);
		CHECK_INT(1, p2->val, 6);

		p2 = p1;
		p3 = p1;
		p2->val = 7;
		DBG("\n%d, %d", p1->val, p2->val);
		CHECK_INT(2, p3->val, 4);
		CHECK_INT(3, p2->val, 7);

		CHECK_INT(4, CData::NumInstances(), 3);
	}
	CHECK_INT(5, CData::NumInstances(), 0);
	return res;
}
