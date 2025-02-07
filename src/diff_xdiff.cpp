extern "C" {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#include <xdiff/xinclude.h>
#pragma GCC diagnostic pop
}

#include <cassert>
#include <stdexcept>
#include <string_view>
#include <vector>

unsigned long enum XDFLAGS {
    NEED_MINIMAL = (1 << 0),

    IGNORE_WHITESPACE = (1 << 1),
    IGNORE_WHITESPACE_CHANGE = (1 << 2),
    IGNORE_WHITESPACE_AT_EOL = (1 << 3),
    IGNORE_CR_AT_EOL = (1 << 4),
    WHITESPACE_FLAGS = (IGNORE_WHITESPACE |
	    		        IGNORE_WHITESPACE_CHANGE |
	    		        IGNORE_WHITESPACE_AT_EOL |
	    		        IGNORE_CR_AT_EOL),

    IGNORE_BLANK_LINES = (1 << 7),

    PATIENCE_DIFF = (1 << 14),
    HISTOGRAM_DIFF = (1 << 15),
    DIFF_ALGORITHM_MASK = (PATIENCE_DIFF | HISTOGRAM_DIFF),

    INDENT_HEURISTIC = (1 << 23)
};

namespace {

template <typename T>
mmfile_t xdmmfile(T data)
{
	return {
		.ptr = (char*)data.data(),
		.size = (long)data.size()
	};
};

std::string_view recsv(xdfile_t & xdf, size_t idx)
{
    auto & rec = *xdf.recs[idx];
	std::string_view result{
		rec.ptr, static_cast<size_t>(rec.size)
	};
	if (result.back() == '\n') {
		result = result.substr(0, result.size()-1);
	}
	return result;
}

#define mustbe0(call) mustbe0_(call, "failed: " #call)
void mustbe0_(int r, char const * msg)
{
    if (r != 0) { throw std::runtime_error(msg); }
}

}

enum DiffType { EQUAL, INSERT, DELETE };
struct Diff {
    DiffType type;
    std::string_view text;

    Diff(DiffType type, std::string_view text)
    : type(type), text(text) {}
    Diff(DiffType type, xdfile_t & xdf, size_t & line)
    : type(type), text(recsv(xdf, line)) {}
    bool operator==(Diff const&) const = default;
};

class XDiff
{
public:
    template <typename T>
    XDiff(
        T data_old,
        T data_new,
        unsigned long xdflags = NEED_MINIMAL | HISTOGRAM_DIFF | INDENT_HEURISTIC
    ) : xp{
        .flags = xdflags,
        .ignore_regex = nullptr,
        .ignore_regex_nr = 0,
        .anchors = nullptr,
        .anchors_nr = 0,
    }
    {
        mmfile_t o = xdmmfile(data_old), n = xdmmfile(data_new);
        mustbe0(xdl_do_diff(&o, &n, &xp, &xe));
    }
    std::vector<Diff> & edits(std::vector<Diff> & result)
    {
        size_t l1 = 0, l2 = 0;
    	auto* cp1 = xe.xdf1.rchg,* cp2 = xe.xdf2.rchg;
    	ssize_t s1 = xe.xdf1.nrec, s2 = xe.xdf2.nrec;
    	while ((ssize_t)l1 < s1 || (ssize_t)l2 < s2) {
    		auto c1 = ((ssize_t)l1 < s1 ? (cp1[l1]?1:0) : 0);
    		auto c2 = ((ssize_t)l2 < s2 ? (cp2[l2]?1:0) : 0);
    		switch ((c1 << 2) | c2) {
    		case (1 << 2) | 1:
    			// remove and add
                result.push_back(Diff(DELETE, xe.xdf1, l1));
                ++ l1;
                result.push_back(Diff(INSERT, xe.xdf2, l2));
                ++ l2;
    			break;
    		case (1 << 2) | 0:
    			// remove only
                result.push_back(Diff(DELETE, xe.xdf1, l1));
                ++ l1;
    			break;
    		case 0 | 1:
    			// add only
                result.push_back(Diff(INSERT, xe.xdf2, l2));
                ++ l2;
    			break;
    		case 0:
    			// same
    			assert(recsv(xe.xdf1,l1)==recsv(xe.xdf2,l2));
                result.push_back(Diff(EQUAL, xe.xdf1, l1));
                ++ l1;
                ++ l2;
    			break;
    		}
    	}
        return result;
    }
    ~XDiff()
    {
        xdl_free_env(&xe);
    }
private:
    xpparam_t xp;
    xdfenv_t xe;
};

#include <iostream>
namespace {

class XDiffAdapter {
public:
    XDiffAdapter() {}

    std::vector<Diff> diff_main(std::string a_, std::string b_, bool)
    {
        std::string a, b;
        for (auto [in,out] : {std::pair(a_,&a),{b_,&b}}) {
            for (auto & ch : in) {
                out->push_back(ch);
                out->push_back('\n');
            }
        }
        std::vector<Diff> result;
        return std::move(XDiff(a, b).edits(result));
    }
};

void assertEquals(char const*desc, std::vector<Diff>&expected, std::vector<Diff>const&actual)
{
    std::cerr << desc << std::endl;
    assert(std::equal(expected.begin(),expected.end(),actual.begin(),actual.end()));
}
template <typename... T>
std::vector<Diff> diffList(T... diffs)
{
    return {diffs...};
}

}

void test_diff_xdiff()
{
  XDiffAdapter dmp;
  std::vector<Diff> diffs;

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
