/* $Id$ */

#define UNITTEST

#include "../../stdafx.h"

EXTERN_C_BEGIN
#include "../../macros.h"
#include "../../tile.h"
#include "../../openttd.h"
#include "../../map.h"
#include "../../rail.h"
EXTERN_C_END

//#include "../track_dir.hpp"

#include "../yapf.hpp"

#include "../autocopyptr.hpp"

#include "unittest.h"

#include "test_autocopyptr.h"
#include "test_binaryheap.h"
#include "test_fixedsizearray.h"
#include "test_blob.h"
#include "test_hashtable.h"
#include "test_yapf.h"

int _total_pf_time_us = 0;

int num_tests_failed = 0;
int num_tests_total = 0;
bool _dbg = false;

int do_test(const char* name, TESTPROC test_proc, bool silent)
{
	printf("%s ", name);
	if (!silent) {printf("[enter]:"); getc(stdin);}
	_dbg = !silent;
	fflush(stdout);
	int res = test_proc(silent);
	if (res == 0)
	{
		printf("%s OK\n", silent ? "..." : "\n");
	}
	else {
		printf("\n ERROR! (0x%X)\n", res);
		printf("\nFailed cases:");
		int num_failed = 0;
		for(int i = 0; i < 32; i++) {
			if (((1 << i) & res) != 0) {
				printf(" %d", i);
				num_failed++;
			}
		}
		printf("\n\nTotal: %d cases failed\n\n", num_failed);
	}

	num_tests_total++;
	if (res != 0) num_tests_failed++;

	return (res == 0) ? 0 : 1;
}

struct TEST_RECORD {
	const char* name;
	TESTPROC    testproc;
};

TEST_RECORD tests[] = {
	{"AutoCopyPtr test"   , &TestAutoCopyPtr         },
	{"BinaryHeap test 1"  , &TestBinaryHeap1         },
	{"BinaryHeap test 2"  , &TestBinaryHeap2         },
	{"FixedSizeArray test", &TestFixedSizeArray      },
	{"Array test"         , &TestArray               },
	{"Blob test 1"        , &TestBlob1               },
	{"Blob test 2"        , &TestBlob2               },
	{"HashTable test 1"   , &TestHashTable1          },
	{"Yapf test 1"        , &CTestYapf1::stTestAstar },
	{"Yapf test 2"        , &CTestYapf2::stTestAstar },

	{NULL                 , NULL                     },
};

int main(int argc, char** argv)
{
	bool silent = (argc == 1);

	for (TEST_RECORD* tr = tests; tr->name != NULL; tr++)
		do_test(tr->name, tr->testproc, silent);

	if (num_tests_failed == 0)
		printf("\nALL %d TESTS PASSED OK!\n\n", num_tests_total);
	else
		printf("\n****** %d (from %d of total) TEST(S) FAILED! ******\n", num_tests_failed, num_tests_total);
	return 0;
}



extern "C"
const TileIndexDiffC _tileoffs_by_dir[] = {
	{-1,  0},
	{ 0,  1},
	{ 1,  0},
	{ 0, -1}
};

extern "C"
const byte _ffb_64[128] = {
	 0,  0,  1,  0,  2,  0,  1,  0,
	 3,  0,  1,  0,  2,  0,  1,  0,
	 4,  0,  1,  0,  2,  0,  1,  0,
	 3,  0,  1,  0,  2,  0,  1,  0,
	 5,  0,  1,  0,  2,  0,  1,  0,
	 3,  0,  1,  0,  2,  0,  1,  0,
	 4,  0,  1,  0,  2,  0,  1,  0,
	 3,  0,  1,  0,  2,  0,  1,  0,

	 0,  0,  0,  2,  0,  4,  4,  6,
	 0,  8,  8, 10,  8, 12, 12, 14,
	 0, 16, 16, 18, 16, 20, 20, 22,
	16, 24, 24, 26, 24, 28, 28, 30,
	 0, 32, 32, 34, 32, 36, 36, 38,
	32, 40, 40, 42, 40, 44, 44, 46,
	32, 48, 48, 50, 48, 52, 52, 54,
	48, 56, 56, 58, 56, 60, 60, 62,
};

/* Maps a trackdir to the (4-way) direction the tile is exited when following
* that trackdir */
extern "C"
const DiagDirection _trackdir_to_exitdir[] = {
	DIAGDIR_NE,DIAGDIR_SE,DIAGDIR_NE,DIAGDIR_SE,DIAGDIR_SW,DIAGDIR_SE, DIAGDIR_NE,DIAGDIR_NE,
	DIAGDIR_SW,DIAGDIR_NW,DIAGDIR_NW,DIAGDIR_SW,DIAGDIR_NW,DIAGDIR_NE,
};

/* Maps a diagonal direction to the all trackdirs that are connected to any
* track entering in this direction (including those making 90 degree turns)
*/
extern "C"
const TrackdirBits _exitdir_reaches_trackdirs[] = {
	TRACKDIR_BIT_X_NE | TRACKDIR_BIT_LOWER_E | TRACKDIR_BIT_LEFT_N,  /* DIAGDIR_NE */
	TRACKDIR_BIT_Y_SE | TRACKDIR_BIT_LEFT_S  | TRACKDIR_BIT_UPPER_E, /* DIAGDIR_SE */
	TRACKDIR_BIT_X_SW | TRACKDIR_BIT_UPPER_W | TRACKDIR_BIT_RIGHT_S, /* DIAGDIR_SW */
	TRACKDIR_BIT_Y_NW | TRACKDIR_BIT_RIGHT_N | TRACKDIR_BIT_LOWER_W  /* DIAGDIR_NW */
};

/* Maps a trackdir to all trackdirs that make 90 deg turns with it. */
extern "C"
const TrackdirBits _track_crosses_trackdirs[] = {
	TRACKDIR_BIT_Y_SE     | TRACKDIR_BIT_Y_NW,                                                   /* TRACK_X     */
	TRACKDIR_BIT_X_NE     | TRACKDIR_BIT_X_SW,                                                   /* TRACK_Y     */
	TRACKDIR_BIT_RIGHT_N  | TRACKDIR_BIT_RIGHT_S  | TRACKDIR_BIT_LEFT_N  | TRACKDIR_BIT_LEFT_S,  /* TRACK_UPPER */
	TRACKDIR_BIT_RIGHT_N  | TRACKDIR_BIT_RIGHT_S  | TRACKDIR_BIT_LEFT_N  | TRACKDIR_BIT_LEFT_S,  /* TRACK_LOWER */
	TRACKDIR_BIT_UPPER_W  | TRACKDIR_BIT_UPPER_E  | TRACKDIR_BIT_LOWER_W | TRACKDIR_BIT_LOWER_E, /* TRACK_LEFT  */
	TRACKDIR_BIT_UPPER_W  | TRACKDIR_BIT_UPPER_E  | TRACKDIR_BIT_LOWER_W | TRACKDIR_BIT_LOWER_E  /* TRACK_RIGHT */
};
