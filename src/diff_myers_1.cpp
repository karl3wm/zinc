
/* One URL for the difference algorithm websearching sees as canonical is http://www.xmailserver.org/diff2.pdf .
 * This algorithm has been heavily improved but is a baseline that introduces ccmmon concepts and terminology, and works well.
 */

#include <cassert>
#include <span>
#include <string_view>
#include <unistd.h> // ssize_t
#include <vector>

using namespace std;

/* Let A = a 1 a 2 . . . a N and B = b1 b2 . . . b M be sequences of length N and M respectively.
 * The edit graph for A and B has a vertex at each point in the grid (x,y), x∈[0,N] and y∈[0,M].
 */ 
/* If a x = by then there is a diagonal edge connecting vertex (x−1,y−1) to vertex (x,y).
 * The points (x,y) for which a x = by are called match points.
 */
class EditGraph
{
public:
    enum Edit {
        DELETE,
        ADD,
        SAME,
        X = DELETE,
        Y = ADD,
        S = SAME,
    };

    template <typename InitialSequence>
    EditGraph(InitialSequence data_old, InitialSequence data_new)
    : N(data_old.size()), M(data_new.size()), MAX(N+M)
    , matches_(N * M)
    {
        Vs.resize(V_idx_(MAX,(ssize_t)MAX) + 1);
        InitialSequence data_lists[2] = {data_old, data_new};
        for (size_t idx = 0; idx < 2; ++ idx) {
            auto & hashes = hashes_[idx];
            auto & datas = data_lists[idx];
            hashes.reserve(datas.size());
            for (auto & item : datas) {
                hashes.emplace_back(hash<decay_t<decltype(item)>>()(item));
            }
        }
        for (size_t x = 0; x < N; ++ x) {
            for (size_t y = 0; y < M; ++ y) {
                match(x, y) = (hashes_[OLD][x] == hashes_[NEW][y]);
            }
        }
    }

    span<Edit> solve() {
        /*
         * Let a D-path be a path starting at (0,0) that has exactly D
         * non-diagonal edges. A 0-path must consist solely of diagonal edges.
         * By a simple induction, it follows that a D-path must consist of a (D
         * − 1)-path followed by a non-diagonal edge and then a possibly empty
         * sequence of diagonal edges called a snake.
         */

        /*
         * Number the diagonals in the grid of edit graph vertices so that
         * diagonal k consists of the points (x,y) for which x − y = k. With
         * this definition the diagonals are numbered from −M to N. Note that a
         * vertical (horizontal) edge - 4 - with start point on diagonal k has
         * end point on diagonal k − 1 (k + 1) and a snake remains on the
         * diagonal in which it starts.
         */

        /*
         * A D-path is furthest reaching in diagonal k if and only if it is one
         * of the D-paths ending on diagonal k whose end point has the greatest
         * possible row (column) number of all such paths. Informally, of all
         * D-paths ending in diagonal k, it ends furthest from the origin,
         * (0,0).
         */

        // basically we are solving the edit graph like a maze, where the
        // entrance is (0,0), the exit is (N,M), and diagonal paths exist only
        // where there are matches.
        //
        // the algorithm considers every possible path in parallel, and stops
        // when one is at the end. they are classified based on how far along
        // each diagonal they have reached. they are unique because each has
        // the same number of non-diagonal moves in each parallel step.
        //
        // noting that:
        // - only diagonals of distance D can be reached
        // - only odd/even daiagonals can be reached with odd/even D

        /*
         *  For D ← 0 to M+N Do
         *      For k ← −D to D in steps of 2 Do
         *          Find the endpoint of the furthest reaching D-path in diagonal k.
         *          If (N,M) is the endpoint Then
         *              The D-path is an optimal solution.
         *              Stop
         */
        /*
         * Constant MAX ∈ [0,M+N]
         * Var V: Array [− MAX .. MAX] of Integer
         * 1. V[1] ← 0
         * 2. For D ← 0 to MAX Do
         * 3.   For k ← −D to D in steps of 2 Do
         * 4.       If k = −D or k ≠ D and V[k − 1] < V[k + 1] Then
         * 5.           x ← V[k + 1]
         * 6.       Else
         * 7.           x ← V[k − 1]+1
         * 8.       y ← x − k
         * 9.       While x < N and y < M and a x + 1 = by + 1 Do (x,y) ← (x+1,y+1)
         * 10.      V[k] ← x
         * 11.      If x ≥ N and y ≥ M Then
         * 12.          Length of an SES is D
         * 13.          Stop
         * 14. Length of an SES is greater than MAX
         */
        if (!MAX) { return {}; }
        ssize_t D, k;
        size_t x = 0, y = 0, x_del, x_add;
        // initial snake
        while (x < N && x < M) {
            if (!match(x,x)) {
                break;
            }
            ++ x;
        }
        y = V_x(0,0) = x;
        for (D = 1; D <= (ssize_t)MAX; ++ D) { // number of non-diagonal edges ie edits
            for (k = -D; k <= D; k += 2) { // each reachable diagonal
                if (k == -D) {
                    x = V_x((size_t)D - 1, k + 1); // add
                } else if (k == D) {
                    x = V_x((size_t)D - 1, k - 1) + 1; // del
                } else {
                    x_del = V_x((size_t)D - 1, k - 1);
                    x_add = V_x((size_t)D - 1, k + 1);
                    if (x_add < x_del) {
                        x = x_add;
                    } else {
                        x = x_del + 1;
                    }
                }
                /*if (k == -D || k != D && x_add < x_del) {
                    // k is best reached by incrementing y from k + 1
                    x = x_add;
                } else {
                    x = x_del + 1;
                }*/
                y = kx2y(k, x);
                while (x < N && y < M && match(x, y)) {
                    // snake further along the diagonal
                    ++ x; ++ y;
                }
                V_x((size_t)D, k) = x;
                if (x >= N && y >= M) {
                    // shortest edit sequence is D
                    break;
                }
            }
        }

        // recover the edit sequence
        // the final element is on the 0th diagonal.
        // we'll backtrack following the shortest sequences
        edits_.resize(MAX);
        //x = N;
        //y = M;
        size_t z = MAX;
        //k = 0;
        for (; D >= 0; -- D) {
            while (match(x-1, y-1)) {
                -- x;
                -- y;
                edits_[--z] = SAME;
            }
            x_del = V_x((size_t)D - 1, k - 1);
            x_add = V_x((size_t)D - 1, k + 1);
            // we could just compare x here
            if (k == -D || (k != D && x_add < x_del)) {
                assert(x_add == x);
                //x = x_add;
                -- y;
                ++ k;
                edits_[--z] = ADD;
            } else {
                assert(x_del == x - 1);
                x = x_del;
                -- k;
                edits_[--z] = DELETE;
            }
            assert(y == kx2y(k, x));
        }

        return {edits_.begin() + (long int)z, edits_.end()};
    }

private:
    static constexpr size_t OLD = 0;
    static constexpr size_t NEW = 1;
    size_t N, M, MAX;
    vector<size_t> hashes_[2];
    vector<bool> matches_;
    decltype(matches_)::reference match(size_t old, size_t new_) {
        assert(old < N);
        assert(new_ < M);
        return matches_[old + new_ * M];
    }

    //bool match(size_t old, size_t nw) const
    //{
    //    return matches_[old + nw * hashes_[NEW].size()];
    //}

    /*
     * V is an array of integers where V[k] contains the row index of the
     * endpoint of a furthest reaching path in diagonal k
     */
    // the algorithm describes the path by incrementing D, so keeping a history
    // of V as a function of D lets one recover a path.
    vector<size_t> Vs;

    size_t & V_x(size_t D, ssize_t k)
    {
        auto idx = V_idx_(D, k);
        assert(idx < Vs.size());
        return Vs[idx];
    }
    static size_t kx2y(ssize_t k, size_t x)
    {
        ssize_t y = (ssize_t)x - k;
        assert(y >= 0);
        return (size_t)y;
    }


    size_t V_idx_(size_t D, ssize_t k)
    {
        // the size of each V(D) is D+1 and is indexed by k always-even among [-D,D] 
        // the offset is calculated by the triangular numbers of increasing D
        size_t offset = (D*(D+1)/2);
        ssize_t idx = (k+(ssize_t)D)/2;
        assert(idx >= 0 && (size_t)idx <= D);
        return offset + (size_t)idx;
    }

    vector<Edit> edits_;
};

/*
 * sequence of diagonal edges called a snake.
 */
/*
 * Number the diagonals in the grid of edit graph vertices so that diagonal k
 * consists of the points (x,y) for which x − y = k.
 */

/*
 * The problem of finding a longest common subsequence (LCS) is equivalent to finding a path from (0,0) to (N,M)
 * with the maximum number of diagonal edges. The problem of finding a shortest edit script (SES) is equivalent to
 * finding a path from (0,0) to (N,M) with the minimum number of non-diagonal edges. These are dual problems as a
 * path with the maximum number of diagonal edges has the minimal number of non-diagonal edges (D+2L = M+N).
 */

#define assertEquals(desc, a, b) assert((equal(a.begin(),a.end(),b.begin(),b.end())) && desc)

static struct DMPAdapter
{
    span<EditGraph::Edit> diff_main(string_view a, string_view b, bool)
    {
        return EditGraph(a, b).solve();
    }
} dmp;
#define Diff make_pair<EditGraph::Edit, string_view>
template <typename... T>
vector<EditGraph::Edit> diffList(T... ts)
{
    vector<EditGraph::Edit> result;
    std::pair<EditGraph::Edit, string_view> pairs[] = { ts... };

    for (auto & [ edit, str ] : pairs) {
        result.resize(result.size() + str.size(), edit);
    }
    return result;
}
#define EQUAL EditGraph::SAME
#define INSERT EditGraph::ADD
#define DELETE EditGraph::DELETE

void test_diff_myers_1()
{
  vector<EditGraph::Edit> diffs = {};
  assertEquals("diff_main: Null case.", diffs, dmp.diff_main("", "", false));

  diffs = diffList(Diff(EQUAL, "abc"));
  assertEquals("diff_main: Equality.", diffs, dmp.diff_main("abc", "abc", false));

  diffs = diffList(Diff(EQUAL, "ab"), Diff(INSERT, "123"), Diff(EQUAL, "c"));
  assertEquals("diff_main: Simple insertion.", diffs, dmp.diff_main("abc", "ab123c", false));

  diffs = diffList(Diff(EQUAL, "a"), Diff(DELETE, "123"), Diff(EQUAL, "bc"));
  assertEquals("diff_main: Simple deletion.", diffs, dmp.diff_main("a123bc", "abc", false));

  diffs = diffList(Diff(EQUAL, "a"), Diff(INSERT, "123"), Diff(EQUAL, "b"), Diff(INSERT, "456"), Diff(EQUAL, "c"));
  assertEquals("diff_main: Two insertions.", diffs, dmp.diff_main("abc", "a123b456c", false));

  diffs = diffList(Diff(EQUAL, "a"), Diff(DELETE, "123"), Diff(EQUAL, "b"), Diff(DELETE, "456"), Diff(EQUAL, "c"));
  assertEquals("diff_main: Two deletions.", diffs, dmp.diff_main("a123b456c", "abc", false));

  diffs = diffList(Diff(DELETE, "a"), Diff(INSERT, "b"));
  assertEquals("diff_main: Simple case #1.", diffs, dmp.diff_main("a", "b", false));

  diffs = diffList(Diff(DELETE, "Apple"), Diff(INSERT, "Banana"), Diff(EQUAL, "s are a"), Diff(INSERT, "lso"), Diff(EQUAL, " fruit."));
  assertEquals("diff_main: Simple case #2.", diffs, dmp.diff_main("Apples are a fruit.", "Bananas are also fruit.", false));

  //diffs = diffList(Diff(DELETE, "a"), Diff(INSERT, QString::fromWCharArray((const wchar_t*) L"\u0680", 1)), Diff(EQUAL, "x"), Diff(DELETE, "\t"), Diff(INSERT, QString::fromWCharArray((const wchar_t*) L"\000", 1)));
  //assertEquals("diff_main: Simple case #3.", diffs, dmp.diff_main("ax\t", QString::fromWCharArray((const wchar_t*) L"\u0680x\000", 3), false));

  diffs = diffList(Diff(DELETE, "1"), Diff(EQUAL, "a"), Diff(DELETE, "y"), Diff(EQUAL, "b"), Diff(DELETE, "2"), Diff(INSERT, "xab"));
  assertEquals("diff_main: Overlap #1.", diffs, dmp.diff_main("1ayb2", "abxab", false));

  diffs = diffList(Diff(INSERT, "xaxcx"), Diff(EQUAL, "abc"), Diff(DELETE, "y"));
  assertEquals("diff_main: Overlap #2.", diffs, dmp.diff_main("abcy", "xaxcxabc", false));

  diffs = diffList(Diff(DELETE, "ABCD"), Diff(EQUAL, "a"), Diff(DELETE, "="), Diff(INSERT, "-"), Diff(EQUAL, "bcd"), Diff(DELETE, "="), Diff(INSERT, "-"), Diff(EQUAL, "efghijklmnopqrs"), Diff(DELETE, "EFGHIJKLMNOefg"));
  assertEquals("diff_main: Overlap #3.", diffs, dmp.diff_main("ABCDa=bcd=efghijklmnopqrsEFGHIJKLMNOefg", "a-bcd-efghijklmnopqrs", false));

  diffs = diffList(Diff(INSERT, " "), Diff(EQUAL, "a"), Diff(INSERT, "nd"), Diff(EQUAL, " [[Pennsylvania]]"), Diff(DELETE, " and [[New"));
  assertEquals("diff_main: Large equality.", diffs, dmp.diff_main("a [[Pennsylvania]] and [[New", " and [[Pennsylvania]]", false));
}
