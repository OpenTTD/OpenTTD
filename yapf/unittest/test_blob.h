/* $Id$ */

static int TestBlob1(bool silent)
{
	typedef CBlobT<int64> Blob;
	int res = 0;
	{
		Blob a;
		Blob b;
		CHECK_INT(0, a.IsEmpty(), true);
		CHECK_INT(1, a.Size(), 0);

		const int nItems = 10;

    {
		  for (int i = 1; i <= nItems; i++) {
			  a.Append(i);
			  CHECK_INT(2, a.IsEmpty(), false);
			  CHECK_INT(3, a.Size(), i);
		  }
    }

    {
		  for (int i = 1; i <= nItems; i++) {
			  CHECK_INT(4, *a.Data(i - 1), i);
		  }
    }
	}
	return res;
}

static int TestBlob2(bool silent)
{
	typedef CBlobT<CFsaItem> Blob;
	int res = 0;
	{
		Blob a;
		Blob b;
		CHECK_INT(0, a.IsEmpty(), true);
		CHECK_INT(1, a.Size(), 0);

		const int nItems = 10;

    {
		  for (int i = 1; i <= nItems; i++) {
			  a.Append(CFsaItem(i));
			  CHECK_INT(2, a.IsEmpty(), false);
			  CHECK_INT(3, a.Size(), i);
		  }
    }
    {
		  for (int i = 1; i <= nItems; i++) {
			  CHECK_INT(4, a.Data(i - 1)->i, i);
		  }
    }
		CHECK_INT(15, CFsaItem::NumInstances(), nItems);
	}
	CHECK_INT(16, CFsaItem::NumInstances(), 0);

	return res;
}
