/* $Id$ */

// this test uses CData structure defined in test_autocopyptr.h

static int TestBinaryHeap1(bool silent)
{
	CData::NumInstances() = 0;
	int res = 0;
	{
		const int max_items = 10000;
		const int total_adds = 1000000;
		CBinaryHeapT<CData> bh(max_items);
		CFixedSizeArrayT<CData, max_items> data;


		DBG("\nFilling BinaryHeap with %d items...", max_items);
		CHECK_INT(0, bh.Size(), 0);
		CHECK_INT(1, CData::NumInstances(), 0);
		int i = 0;
		for (; i < max_items; i++) {
			CData& d = data.Add();
			d.val = rand() & 0xFFFF;
			bh.Push(d);
		}
		CHECK_INT(2, bh.Size(), max_items);
		CHECK_INT(3, CData::NumInstances(), max_items);


		DBG("\nShaking items %d times...", total_adds);
		int num_last = bh.GetHead().val;
		for (i = 0; i < total_adds; i++) {
			CData& d = bh.PopHead();
			//printf("\nd->val = %d, num_last = %d", d->val, num_last);
			CHECK_INT(4, d.val < num_last, 0);
			if(d.val < num_last) {
				printf("Sort error @ item %d", i);
			}
			num_last = d.val;
			d.val += rand() & 0xFFFF;
			bh.Push(d);
		}


		DBG("\nDone!");
		CHECK_INT(5, bh.Size(), max_items);
		CHECK_INT(6, CData::NumInstances(), max_items);
	}
	CHECK_INT(7, CData::NumInstances(), 0);
	return res;
}



// this test uses CData and PData structures defined in test_autocopyptr.h

static int TestBinaryHeap2(bool silent)
{
	CData::NumInstances() = 0;
	int res = 0;
	{
		const int max_items = 10000;
		const int total_adds = 1000000;
		CBinaryHeapT<CData> bh(max_items);
		CFixedSizeArrayT<CData, max_items> data;


		DBG("\nFilling BinaryHeap with %d items...", max_items);
		CHECK_INT(0, bh.Size(), 0);
		CHECK_INT(1, CData::NumInstances(), 0);
		int i = 0;
		for (; i < max_items; i++) {
			CData& d = data.Add();
			d.val = rand() & 0xFFFF;
			bh.Push(d);
		}
		CHECK_INT(2, bh.Size(), max_items);
		CHECK_INT(3, CData::NumInstances(), max_items);


		DBG("\nShaking items %d times...", total_adds);
		int num_last = bh.GetHead().val;
		for (i = 0; i < total_adds; i++) {
			CData& d = bh.GetHead();
			bh.RemoveHead();
			//printf("\nd->val = %d, num_last = %d", d->val, num_last);
			CHECK_INT(4, d.val < num_last, 0);
			if(d.val < num_last) {
				printf("Sort error @ item %d", i);
			}
			num_last = d.val;
			d.val += rand() & 0xFFFF;
			bh.Push(d);
		}


		DBG("\nDone!");
		CHECK_INT(5, bh.Size(), max_items);
		CHECK_INT(6, CData::NumInstances(), max_items);
	}
	CHECK_INT(7, CData::NumInstances(), 0);
	return res;
}

