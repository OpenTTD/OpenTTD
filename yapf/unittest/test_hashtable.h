/* $Id$ */

struct CHashItem1 {
	struct CKey {
		int k;
		FORCEINLINE int CalcHash() const {return k;};
		FORCEINLINE bool operator == (const CKey& other) const {return (k == other.k);}
	};
	typedef CKey Key;
	CKey  key;
	int   val;
	CHashItem1* m_next;

	CHashItem1() : m_next(NULL) {}
	FORCEINLINE const Key& GetKey() const {return key;}
	CHashItem1* GetHashNext() {return m_next;}
	void SetHashNext(CHashItem1* next) {m_next = next;}
};

static int TestHashTable1(bool silent)
{
	typedef CHashItem1 Item;
	typedef CHashTableT<Item, 12> HashTable1_t;
	typedef CArrayT<Item, 1024, 16384> Array_t;
	typedef CHashTableT<Item, 16> HashTable2_t;

	int res = 0;
	{
		HashTable1_t ht1;
		HashTable2_t ht2;
		Array_t      ar1;
		Array_t      ar2;

#ifdef _DEBUG
		static const int nItems = 10000;
#else
		static const int nItems = 1000000;
#endif
		{
			srand(0);
			for (int i = 0; i < nItems; i++) {
				int r1 = i;
				int r2 = rand() & 0x0000FFFF | (rand() << 16);
				Item& I1 = ar1.Add();
				Item& I2 = ar2.Add();
				I1.key.k = r1;
				I2.key.k = r1;
				I1.val = r2;
				I2.val = r2;
				ht1.Push(I1);
				ht2.Push(I2);
			}
		}
		{
			srand(0);
			for (int i = 0; i < nItems; i++) {
				int r1 = i;
				int r2 = rand() & 0x0000FFFF | (rand() << 16);
				HashTable1_t::Tkey k; k.k = r1;
				Item& i1 = ht1.Find(k);
				Item& i2 = ht2.Find(k);

				CHECK_INT(0, &i1 != NULL, 1);
				CHECK_INT(1, &i2 != NULL, 1);

				if (&i1 != NULL) CHECK_INT(2, i1.val, r2);
				if (&i2 != NULL) CHECK_INT(3, i2.val, r2);
			}
		}
	}
	return res;
}
