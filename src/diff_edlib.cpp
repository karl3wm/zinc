//#pragma hdrstop "../third_party/edlib/edlib/include"
//#pragma include_path("../third_party/edlib/edlib/include")
#include "edlib.h"
//#include "edlib.cpp"
#include <cassert>
#include <string>
#include <vector>
#include <stdexcept>

enum DiffType { EQUAL, INSERT, DELETE };
struct Diff {
    DiffType type;
    std::string text;

    Diff(DiffType type, std::string text)
    : type(type), text(text) {}
    bool operator==(Diff const&) const = default;
};

class EdlibAdapter {
public:
    EdlibAdapter() {}

    std::vector<Diff> diff_main(std::string a_, std::string b_, bool) {
        //unsigned char* const a = reinterpret_cast<unsigned char*>(const_cast<char*>(a_.c_str()));
        //unsigned char* const b = reinterpret_cast<unsigned char*>(const_cast<char*>(b_.c_str()));
        //int aLen = a_.size();
        //int bLen = b_.size();

        EdlibAlignResult align = edlibAlign(b_.data(), (int)b_.size(), a_.data(), (int)a_.size(), edlibNewAlignConfig(-1, EDLIB_MODE_NW, EDLIB_TASK_PATH, nullptr, 0));
        if (align.status != EDLIB_STATUS_OK) {
            throw std::runtime_error("edlibAlign failed");
        }

        std::vector<Diff> diffs;

        if (align.editDistance == 0) {
            assert(b_ == a_);
            if (b_.size()) {
                diffs.push_back({EQUAL, a_});
            }
            return diffs;
        }

        size_t offa = 0, offb = 0;
        size_t acc_del = 0, acc_ins = 0;
        unsigned char last_op = 255;
        unsigned char next_op;
        size_t last_idx = 0;
        if (align.startLocations[0] > 0) {
            offa = offb = (size_t)align.startLocations[0];
            diffs.push_back({EQUAL, b_.substr(0, offb)});
        }
        for (size_t i = 0; i <= (size_t)align.alignmentLength; ++ i) {
            if (i < (size_t)align.alignmentLength) {
                next_op = align.alignment[i];
                switch (next_op) {
                case EDLIB_EDOP_MATCH:
                    break;
                case EDLIB_EDOP_INSERT:
                case EDLIB_EDOP_DELETE:
                case EDLIB_EDOP_MISMATCH:
                    switch (next_op) {
                    case EDLIB_EDOP_INSERT:
                        ++ acc_ins;
                        break;
                    case EDLIB_EDOP_DELETE:
                        ++ acc_del;
                        break;
                    case EDLIB_EDOP_MISMATCH:
                        ++ acc_ins;
                        ++ acc_del;
                        break;
                    }
                    next_op = EDLIB_EDOP_MISMATCH;
                    break;
                default:
                    throw "invalid op";
                }
                if (i == 0) {
                    last_op = next_op;
                }
                if (next_op == last_op) {
                    continue;
                }
            }
            size_t ct = i - last_idx;
            assert(ct > 0);
            switch (last_op) {
            case EDLIB_EDOP_MATCH:
                diffs.push_back({EQUAL, a_.substr(offa, ct)});
                offa += ct; offb += ct;
                break;
            case EDLIB_EDOP_MISMATCH:
                if (acc_del) {
                    diffs.push_back({DELETE, a_.substr(offa, acc_del)});
                    offa += acc_del;
                    acc_del = 0;
                }
                if (acc_ins) {
                    diffs.push_back({INSERT, b_.substr(offb, acc_ins)});
                    offb += acc_ins;
                    acc_ins = 0;
                }
                break;
            default:
                throw "unexpected op; logic error in adapter algorithm";
            }
            last_op = next_op;
            last_idx = i;
        }
        assert((size_t)align.endLocations[0] + 1 == offa);
        ssize_t ct = (ssize_t)std::min(a_.size() - offa, b_.size() - offb);
        if (ct) {
            diffs.push_back({EQUAL, b_.substr(offb, (size_t)ct)});
            offb = (size_t)((ssize_t)(offb) + ct);
            offa = (size_t)((ssize_t)(offa) + ct);
        }
        assert(offa == a_.size() || offb == b_.size());
        if (offa < a_.size()) {
            diffs.push_back({DELETE, a_.substr(offa)});
        } else if (offb < b_.size()) {
            diffs.push_back({INSERT, b_.substr(offb)});
        }
        edlibFreeAlignResult(align);
        return diffs;
    }

private:
};

#include <iostream>
//#define assertEquals(desc, a, b) assert((equal(a.begin(),a.end(),b.begin())) && desc)
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

int main()
{
  EdlibAdapter dmp;
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


/*
#include <iostream>
int main() {
    std::cout << "kitten -> sitting" << std::endl;
    EdlibAdapter adapter("kitten", "sitting");
    std::vector<EdlibAdapter::Diff> diffs = adapter.getDiffs();

    for (const auto& diff : diffs) {
        switch (diff.type) {
            case EdlibAdapter::Diff::EQUAL:
                std::cout << "EQUAL: " << diff.text << std::endl;
                break;
            case EdlibAdapter::Diff::INSERT:
                std::cout << "INSERT: " << diff.text << std::endl;
                break;
            case EdlibAdapter::Diff::DELETE:
                std::cout << "DELETE: " << diff.text << std::endl;
                break;
        }
    }

    return 0;
}
*/
