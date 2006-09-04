/* $Id$ */

#include "../yapf_base.hpp"

struct CYapfMap1
{
	enum {xMax = 32, yMax = 68};
	static int MapZ(int x, int y)
	{
		static const char *z1[yMax] = {
			"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A000000000001000000000000000000000000000000000000000000000000000000A",
			"A000000000001000000000000000000000000000000000000000000000000000000A",
			"A000033333333333000000000000000000000000000000000000000000000000000A",
			"A000030000000000000000000000000000000000000000000000000000000000000A",
			"A000030000000000000000000000000000000000000000000000000000000000000A",
			"A000030000000000000000000000000000000000000000000000000000000000000A",
			"A000030000000000000000000000000000000000000000000000000000000000000A",
			"A000030000000000000000000000000000000000000000000000000000000000000A",
			"A210030000000000000000000000000000000000000000000000000000000000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A011333323333333233333333333333333333333333333333333333333333000000A",
			"A000030000000000000000000000000000000000000000000000000000003000000A",
			"A000030000000000000000000000000000000000000000000000000000003000000A",
			"A000030000000000000000000000000000000000000000000000000000003000000A",
			"A210030000000000000000000000000000000000000000000000000000003000000A",
			"A000030000000000000000000000000000000000000000000000000000003000000A",
			"A000030000000000000000000000000000000000000000000000000000003000000A",
			"A000230000000000000000000000000000000000000000000000000000003000000A",
			"A000030000000000000000000000000000000000000000000000000000003000000A",
			"A000030000000000000000000000000000000000000000000000000000003000000A",
			"A000030000000000000000000000000000000000000000000000000000003000000A",
			"A000000000000000000000000000003333333333333333333333333333333000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
		};

		static const char *z2[yMax] = {
			"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A003333333333333333333333333333333333333333333333333300000000000000A",
			"A003000000001000000000000000000000000000000000000000300000000000000A",
			"A003000000001000000000000000000000000000000000000000300000000000000A",
			"A003333333333333333333333333333333333333300000000000300000000000000A",
			"A000030000000000000000000000000000000000300000000000300000000000000A",
			"A000030000000000000000000000000000000000333333333333300000000000000A",
			"A000030000000000000000000000000000000000300000000000000000000000000A",
			"A000030000000000000000000000000000000000300000000000000000000000000A",
			"A000030000000000000000000000000000000000300000000000000000000000000A",
			"A210030000000000000000000000000000000000300000000000000000000000000A",
			"A000000000000000000000000000000000000000333300000000000000000000000A",
			"A000000000000000000000000000000000000000000300000000000000000000000A",
			"A000000000000000000000000000000000000000000300000000000000000000000A",
			"A000000000000000000000000000000000000000000300000000000000000000000A",
			"A012333323333333233333333333333333333333333333333333333333333000000A",
			"A000030000000000000000000000000000000000000000000000000000003000000A",
			"A000030000000000000000000000000000000000000000000000000300003000000A",
			"A000030000000000000000000000000000000000000000000000000300003000000A",
			"A210030000000000000000000000000000000000000000000000000330003000000A",
			"A000030000000000000000000000000000000000000000000000000300003000000A",
			"A000030000000000000000000000000000000000000000000000000300003000000A",
			"A000230000000000000000000000000000000000000000000000000300003000000A",
			"A000030000000000000000000000000000000000000000000000000300003000000A",
			"A000030000000000000000000000000000000000000000000000000300003000000A",
			"A000030000000000000000000000000000000000000000000000000300003000000A",
			"A000000000000000000000000000003333333333333333333333333333333000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"A000000000000000000000000000000000000000000000000000000000000000000A",
			"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
		};

		static const char **z = z1;

		if (x >= 0 && x < xMax && y >= 0 && y < yMax) {
			int ret = z[x][y];
			return ret;
		}
		return z[0][0];
	}
};

struct CNodeKey1 {
	int            m_x;
	int            m_y;
	Trackdir       m_td;
	DiagDirection  m_exitdir;

	CNodeKey1() : m_x(0), m_y(0), m_td(INVALID_TRACKDIR) {}

	int CalcHash() const {return m_x | (m_y << 5) | (m_td << 10);}
	bool operator == (const CNodeKey1& other) const {return (m_x == other.m_x) && (m_y == other.m_y) && (m_td == other.m_td);}
};

struct CNodeKey2 : public CNodeKey1
{
	int CalcHash() const {return m_x | (m_y << 5) | (m_exitdir << 10);}
	bool operator == (const CNodeKey1& other) const {return (m_x == other.m_x) && (m_y == other.m_y) && (m_exitdir == other.m_exitdir);}
};

template <class Tkey_>
struct CTestYapfNodeT {
	typedef Tkey_ Key;
	typedef CTestYapfNodeT<Tkey_> Node;

	Tkey_           m_key;
	CTestYapfNodeT *m_parent;
	int             m_cost;
	int             m_estimate;
	CTestYapfNodeT *m_next;

	CTestYapfNodeT(CTestYapfNodeT* parent = NULL) : m_parent(parent), m_cost(0), m_estimate(0), m_next(NULL) {}

	const Tkey_& GetKey() const {return m_key;}
	int GetCost() {return m_cost;}
	int GetCostEstimate() {return m_estimate;}
	bool operator < (const CTestYapfNodeT& other) const {return m_estimate < other.m_estimate;}
	CTestYapfNodeT* GetHashNext() {return m_next;}
	void SetHashNext(CTestYapfNodeT* next) {m_next = next;}
};

typedef CTestYapfNodeT<CNodeKey1> CYapfNode1;
typedef CTestYapfNodeT<CNodeKey2> CYapfNode2;

template <class Types>
struct CYapfTestBaseT
{
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables
	typedef typename Types::Map Map;

	int m_x1, m_y1;
	int m_x2, m_y2;
	Trackdir m_td1;

	CYapfTestBaseT()
		: m_x1(0), m_y1(0), m_x2(0), m_y2(0), m_td1(INVALID_TRACKDIR)
	{
	}

	void Set(int x1, int y1, int x2, int y2, Trackdir td1)
	{
		m_x1 = x1;
		m_y1 = y1;
		m_x2 = x2;
		m_y2 = y2;
		m_td1 = td1;
	}

	/// to access inherited path finder
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}
	FORCEINLINE char TransportTypeChar() const {return 'T';}

	/** Called by YAPF to move from the given node to the next tile. For each
	 *  reachable trackdir on the new tile creates new node, initializes it
	 *  and adds it to the open list by calling Yapf().AddNewNode(n) */
	FORCEINLINE void PfFollowNode(Node& org)
	{
		int x_org = org.m_key.m_x;
		int y_org = org.m_key.m_y;
		int z_org = Map::MapZ(x_org, y_org);
		DiagDirection exitdir = TrackdirToExitdir(org.m_key.m_td);

		TileIndexDiffC diff = TileIndexDiffCByDir(exitdir);
		int x_new = x_org + diff.x;
		int y_new = y_org + diff.y;
		int z_new = Map::MapZ(x_new, y_new);

		int z_diff = z_new - z_org;
		if (abs(z_diff) > 1) return;

		TrackdirBits trackdirs = DiagdirReachesTrackdirs(exitdir);
		TrackdirBits trackdirs90 = TrackdirCrossesTrackdirs(org.m_key.m_td);
		trackdirs &= (TrackdirBits)~(int)trackdirs90;

		while (trackdirs != TRACKDIR_BIT_NONE) {
			Trackdir td_new = (Trackdir)FindFirstBit2x64(trackdirs);
			trackdirs = (TrackdirBits)KillFirstBit2x64(trackdirs);

			Node& n = Yapf().CreateNewNode();
			n.m_key.m_x  =  x_new;
			n.m_key.m_y  =  y_new;
			n.m_key.m_td = td_new;
			n.m_key.m_exitdir = TrackdirToExitdir(n.m_key.m_td);
			n.m_parent   = &org;
			Yapf().AddNewNode(n);
		}
	}

	/// Called when YAPF needs to place origin nodes into open list
	FORCEINLINE void PfSetStartupNodes()
	{
		Node& n1 = Yapf().CreateNewNode();
		n1.m_key.m_x = m_x1;
		n1.m_key.m_y = m_y1;
		n1.m_key.m_td = m_td1;
		n1.m_key.m_exitdir = TrackdirToExitdir(n1.m_key.m_td);
		Yapf().AddStartupNode(n1);
	}

	/** Called by YAPF to calculate the cost from the origin to the given node.
	 *  Calculates only the cost of given node, adds it to the parent node cost
	 *  and stores the result into Node::m_cost member */
	FORCEINLINE bool PfCalcCost(Node& n)
	{
		// base tile cost depending on distance
		int c = IsDiagonalTrackdir(n.m_key.m_td) ? 10 : 7;
		// additional penalty for curve
		if (n.m_parent != NULL && n.m_key.m_td != n.m_parent->m_key.m_td) c += 3;
		// z-difference cost
		int z_new = Map::MapZ(n.m_key.m_x, n.m_key.m_y);
		int z_old = Map::MapZ(n.m_parent->m_key.m_x, n.m_parent->m_key.m_y);
		if (z_new > z_old) n.m_cost += (z_new - z_old) * 10;
		// apply it
		n.m_cost = n.m_parent->m_cost + c;
		return true;
	}

	/** Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::m_estimate */
	FORCEINLINE bool PfCalcEstimate(Node& n)
	{
		int dx = abs(n.m_key.m_x - m_x2);
		int dy = abs(n.m_key.m_y - m_y2);
		int dd = min(dx, dy);
		int dxy = abs(dx - dy);
		int d = 14 * dd + 10 * dxy;
		n.m_estimate = n.m_cost + d /*+ d / 4*/;
		return true;
	}

	/// Called by YAPF to detect if node ends in the desired destination
	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		bool bDest = (n.m_key.m_x == m_x2) && (n.m_key.m_y == m_y2);
		return bDest;
	}

	static int stTestAstar(bool silent)
	{
		Tpf pf;
		pf.Set(3, 3, 20, 56, TRACKDIR_X_NE);
		int ret = pf.TestAstar(silent);
		return ret;
	}

	int TestAstar(bool silent)
	{
		CPerformanceTimer pc;

		pc.Start();
		bool bRet = Yapf().FindPath(NULL);
		pc.Stop();

		if (!bRet) return 1;

		typedef CFixedSizeArrayT<int, 1024> Row;
		typedef CFixedSizeArrayT<Row, 1024> Box;

		Box box;
		{
			for (int x = 0; x < Map::xMax; x++) {
				Row& row = box.Add();
				for (int y = 0; y < Map::yMax; y++) {
					row.Add() = Map::MapZ(x, y);
				}
			}
		}
		int nPathTiles = 0;
		{
			for (Node* pNode = &Yapf().GetBestNode(); pNode != NULL; pNode = pNode->m_parent) {
				box[pNode->m_key.m_x][pNode->m_key.m_y] = '.';
				nPathTiles++;
			}
		}
		{
			printf("\n\n");
			for (int x = 0; x < Map::xMax; x++) {
				for (int y = 0; y < Map::yMax; y++) {
					printf("%c", box[x][y]);
				}
				printf("\n");
			}
		}

		{
			printf("\n");
			printf("Path Tiles:    %6d\n", nPathTiles);
//			printf("Closed nodes:  %6d\n", pf.m_nodes.ClosedCount());
//			printf("Open nodes:    %6d\n", pf.m_nodes.OpenCount());
//			printf("A-star rounds: %6d\n", pf.m_num_steps);
		}
		int total_time = pc.Get(1000000);
		if (total_time != 0)
			printf("Total time:    %6d us\n", pc.Get(1000000));

		printf("\n");

		{
			int nCnt = Yapf().m_nodes.TotalCount();
			for (int i = 0; i < nCnt; i++) {
				Node& n = Yapf().m_nodes.ItemAt(i);
				int& z = box[n.m_key.m_x][n.m_key.m_y];
				z = (z < 'a') ? 'a' : (z + 1);
			}
		}
		{
			for (int x = 0; x < Map::xMax; x++) {
				for (int y = 0; y < Map::yMax; y++) {
					printf("%c", box[x][y]);
				}
				printf("\n");
			}
		}

		return 0;
	}
};

struct CDummy1 {};
struct CDummy2 {};
struct CDummy3 {};

template <class Tpf_, class Tnode_list, class Tmap>
struct CYapf_TypesT
{
	typedef CYapf_TypesT<Tpf_, Tnode_list, Tmap> Types;

	typedef Tpf_                              Tpf;
	typedef Tnode_list                        NodeList;
	typedef Tmap                              Map;
	typedef CYapfBaseT<Types>                 PfBase;
	typedef CYapfTestBaseT<Types>             PfFollow;
	typedef CDummy1                           PfOrigin;
	typedef CDummy2                           PfDestination;
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;
	typedef CDummy3                           PfCost;
};

typedef CNodeList_HashTableT<CYapfNode1, 12, 16> CNodeList1;
typedef CNodeList_HashTableT<CYapfNode2, 12, 16> CNodeList2;

struct CTestYapf1
	: public CYapfT<CYapf_TypesT<CTestYapf1, CNodeList1, CYapfMap1> >
{
};

struct CTestYapf2
	: public CYapfT<CYapf_TypesT<CTestYapf2, CNodeList2, CYapfMap1> >
{
};
