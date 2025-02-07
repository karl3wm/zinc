#include "dmp_diff.hpp"


#define assertEquals(desc, a, b) assert((equal(a.begin(),a.end(),b.begin(), diff_eq)) && desc)


static struct DMPAdapter : public MyersDiff<string_view>
{
    DMPAdapter()
    : MyersDiff({},{})
    {
        Diff_Timeout = 0;
    }
    using MyersDiff::diff_main;
    auto diff_main(string_view a, string_view b, bool)
    {
        auto result = this->diff_main(Range(a), Range(b));
        return result;
    }
} dmp;
DMPAdapter::Diff Diff(Operation op, string_view txt)
{
    return DMPAdapter::Diff(op, DMPAdapter::Range(txt));
}
template<typename... Ds>
DMPAdapter::Diffs diffList(Ds &&... ds)
{
    return {std::move(ds)...};
}
auto diff_eq = [](const DMPAdapter::Diff& lhs, const DMPAdapter::Diff& rhs) {
    assert(lhs.operation == rhs.operation);
    string_view ltext(lhs.text.from, lhs.text.till);
    string_view rtext(rhs.text.from, rhs.text.till);
    assert(ltext == rtext);
    //assert(string_view(lhs.text.from, lhs.text.till) ==
    //       string_view(rhs.text.from, rhs.text.till));
    //return lhs.operation == rhs.operation &&
    //    string_view(lhs.text.from, lhs.text.till) ==
    //    string_view(rhs.text.from, rhs.text.till);
    return true;
};
/*
#define Diff make_pair<EditGraph::Edit, string_view>
*/
/*
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
*/

//extern decltype(dmp.diff_main("","",false)) diffs;
decltype(dmp.diff_main("","",false)) diffs = {};

void test_diff_gritzko()
{
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

