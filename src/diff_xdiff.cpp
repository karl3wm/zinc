#include <zinc/common.hpp>

#include <cstdlib>
#include <stdlib.h>


extern "C" {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#include <xdiff/xinclude.h>

// add casts in order to navigate -fpermissive void* warnings
} struct AnyPtr { template <class T>
    operator T*() { return (T*) ptr; } void *ptr;
}; extern "C" {
#undef NULL
#define NULL AnyPtr(nullptr)

#undef xdl_malloc
#define xdl_malloc(x) AnyPtr(malloc(x))
#undef xdl_calloc
#define xdl_calloc(n, sz) AnyPtr(calloc(n, sz))
#undef xdl_free
#define xdl_free(ptr) free(ptr)
#undef xdl_realloc
#define xdl_realloc(ptr, x) (decltype(ptr))realloc(ptr, x)

#define xdl_mmfile_first(mmf,size) AnyPtr(xdl_mmfile_first(mmf,size))
#define xdl_cha_alloc(cha) AnyPtr(xdl_cha_alloc(cha))
#define xdl_alloc_grow_helper(p,nr,alloc,size) AnyPtr(xdl_alloc_grow_helper(p,nr,alloc,size))
// done adding casts

#include <xdiff/xprepare.c>

#undef calloc
#undef malloc
#pragma GCC diagnostic pop
}

#include <algorithm>
#include <cassert>
#include <cmath>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <boost/container/devector.hpp>

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
mmfile_t to_mmfile(T data)
{
	return {
		.ptr = (char*)data.data(),
		.size = (long)data.size()
	};
};
template <size_t total=2, typename T>
mmfile_t* to_mmfilep(T data)
{
    static thread_local std::vector<mmfile_t> mms(total);
    mms.emplace_back(to_mmfile(data));
    return &mms.back();
}

std::string_view xdfile_line(xdfile_t & xdf, size_t idx)
{
    auto & rec = *xdf.recs[idx];
	std::string_view result{
		rec.ptr, static_cast<size_t>(rec.size)
	};
	if (result.size()) {
        if (result.back() == '\n') {
            result = result.substr(0, result.size()-1);
        }
	}
	return result;
}

#define mustbe0(call) _0(0 == (call), "failed: " #call)
#define cantbe0(call) _0(0 != (call), "failed: " #call)
inline void _0(bool success, char const * msg)
{ if (!success) { throw std::runtime_error(msg); } }

}

enum DiffType { EQUAL, INSERT, DELETE };
struct Diff {
    DiffType type;
    std::string_view text;

    Diff(DiffType type, std::string_view text)
    : type(type), text(text) {}
    Diff(DiffType type, xdfile_t & xdf, size_t line = 0)
    : type(type), text(xdfile_line(xdf, line)) {}
    bool operator==(Diff const&) const = default;
};

class ChaStore
{
    // a chastore is a linked list of chanodes
    // .head, .tail -> node pointers
    // .isize -> size of each user item allocated in a node
    // .nsize -> size of each node memory region (minus chanode size)
    // .ancur -> pointer to active node being filled
    // a chanode is a header to an allocated memory region, 
    //      .next -> pointer to next node
    //      .icurr -> offset within this node to next use

    // questions: it looks to me like always ancur == tail. true/false?

    // so basically a chanode is a dynamic array similar to a deque
    // it does not appear to manage user offsets within its storage
};

class DynamicXDFile
{
public:
    DynamicXDFile(unsigned int pass, xdfile_t * xdf, xdlclassifier_t * cf, long line_estimate)
    : xdf(xdf), cf(cf), pass(pass)
    {
        /* xdl_prepare_ctx */
	    mustbe0(xdl_cha_init(&xdf->rcha, sizeof(xrecord_t), line_estimate / 4 + 1));

        hbits = xdl_hashbits((unsigned int) line_estimate);
        rhash.resize(1 << hbits);

        xdf->nrec = 0;
        xdf->nreff = 0;
        xdf->dstart = 0;

        /* xdl_cleanup_records */
        if ((mlim = xdl_bogosqrt(line_estimate)) > XDL_MAX_EQLIMIT)
            mlim = XDL_MAX_EQLIMIT;
    }
    long line_estimate() {
        return xdf->rcha.nsize / xdf->rcha.isize;
    }
    size_t size() {
        return (size_t)xdf->nrec;
    }
    std::string_view operator[](size_t line) {
        auto & rec = *recs[line];
        return {rec.ptr, (size_t)rec.size};
    }
    void shift_line_ptrs(ptrdiff_t offset)
    {
        for (auto * rec : recs) {
            rec->ptr += offset;
        }
    }
    void extend_pre(xpparam_t * xpp, long line_estimate, /*std::span<char>*/ std::string_view data)
    {
        /* xdl_prepare_ctx */
    	long nrec/*, hsize, bsize*/;
    	unsigned long hav;
    	char const /**blk, */*cur, *top, *prev;
        xrecord_t *crec;

        unsigned int narec = (unsigned)recs.size() + (unsigned)line_estimate;

        if (narec >= rhash.size()) {
            // this block always asserts to detect situations
            // where this happens frequently or readily.
            // it's better to estimate more lines at the start, because
            // growing the hash table means reseating everything
            assert(narec < rhash.size() && "line estimate was too low by a factor of at least 2");
            hbits = xdl_hashbits((unsigned int) narec);
            rhash.clear();
            rhash.resize(1 << hbits);
            for (auto rec : recs) {
                size_t hi = XDL_HASHLONG(rec->ha, hbits);
                rhash[hi] = rec;
            }
        }

        recs.resize_back(narec);

        nrec = xdf->nrec;
        for (cur = &*data.begin(), top = &*data.end(); cur < top; ) {
            prev = cur;
            hav = xdl_hash_record(&cur, top, (long)xpp->flags);
            recs.resize_back(narec = (unsigned int)nrec + 1);
            cantbe0(crec = xdl_cha_alloc(&xdf->rcha));
            crec->ptr = prev;
            crec->size = (long) (cur - prev);
            crec->ha = hav;
            recs[(size_t)nrec++] = crec;
            mustbe0(xdl_classify_record(pass, cf, rhash.data(), hbits, crec));
        }

        rchg.resize_back((size_t)nrec + 2);
	    if ((XDF_DIFF_ALG(xpp->flags) != XDF_PATIENCE_DIFF) &&
    	    (XDF_DIFF_ALG(xpp->flags) != XDF_HISTOGRAM_DIFF)) {
            rindex.resize_back((size_t)nrec + 1);
            ha.resize_back((size_t)nrec + 1);
    	}

        xdf->nrec = nrec;
        xdf->recs = recs.begin();
        xdf->hbits = hbits;
        xdf->rhash = rhash.data();
        xdf->rchg = rchg.begin() + 1;
        xdf->rindex = rindex.begin();
        xdf->ha = ha.begin();
        xdf->dend = nrec - 1;
    }
    void extend_post(bool compare_nreff)
    {
        /* ~~ xld_optimize_ctxs */
        unsigned int start = xdf->nreff ? (unsigned int)xdf->rindex[xdf->nreff - 1] + 1 : 0;

        /* xdl_cleanup_records */
        long i, nm, nreff;
        xrecord_t **recs;
        xdlclass_t *rcrec;


        if (!compare_nreff) {
            for (nreff = xdf->nreff, i = start, recs = &xdf->recs[start];
                i <= xdf->dend; i++, recs++, nreff++) {
    			xdf->rindex[nreff] = i;
    			xdf->ha[nreff] = (*recs)->ha;
            }
        } else {
            dis.resize_back((size_t)(xdf->dend + 1));

            for (i = start, recs = &xdf->recs[start]; i <= xdf->dend; i++, recs++) {
                rcrec = cf->rcrecs[(*recs)->ha];
                nm = rcrec ? (pass == 1 ? rcrec->len2 : rcrec->len1) : 0;
                dis[(size_t)i] = (nm == 0) ? 0: (nm >= mlim) ? 2: 1;
            }

            for (nreff = xdf->nreff, i = start, recs = &xdf->recs[start];
                 i <= xdf->dend; i++, recs++) {
    		    if (dis[(size_t)i] == 1 ||
        		    (dis[(size_t)i] == 2 && !xdl_clean_mmatch(dis.data(), i, xdf->dstart, xdf->dend))) {
        			xdf->rindex[nreff] = i;
        			xdf->ha[nreff] = (*recs)->ha;
        			nreff++;
        		} else
        			xdf->rchg[i] = 1;
            }
        }
        xdf->nreff = nreff;
    }
    void consume(long lines = 1)
    {
        /* xdl_trim_ends */
        xdf->dstart += lines;

        /* ... */
        cha_consume_(lines);
        if (lines < xdf->dstart) {
            xdf->dstart -= lines;
        } else {
            xdf->dstart = 0;
        }
        xdf->dend -= lines;
        /* because an nreff heuristic uses the total count of matched lines,
         * it seemed to make sense to not consume classifier entries
        for (long i = 0 ; i < lines; ++ i) {
            classifier_consume_(recs[(size_t)i]);
        }
        */
        recs.erase(recs.begin(), recs.begin() + lines);
        xdf->recs = recs.begin();
        rchg.erase(rchg.begin(), rchg.begin() + lines);
        xdf->rchg = rchg.begin() + 1;
        if (!dis.empty()) {
            dis.erase(dis.begin(), dis.begin() + lines);
        }
        xdf->nrec -= lines;

        /* xdl_cleanup_records */
        long nreff = xdf->nreff, nreff_off;
        for (nreff_off = 0; nreff_off < nreff; ++ nreff_off) {
            if (rindex[(size_t)nreff_off] >= lines) {
                break;
            }
        }
        nreff -= nreff_off;
        // vector would make more sense than valarray for rindex. modern compilers vectorize simple arithmetic in loops.
        //if (rindex_tmp.size() < (size_t)nreff) {
        //    rindex_tmp.resize((size_t)nreff);
        //}
        //rindex_tmp = rindex[std::slice((size_t)nreff_off, (size_t)nreff, 1)];
        //rindex_tmp -= lines;
        //rindex[std::slice(0, (size_t)nreff, 1)] = rindex_tmp;
        rindex.erase(rindex.begin(), rindex.begin() + nreff_off);
        // uhh does this get vectorized? or is it too in-place?
        for (size_t i = 0; i < (size_t)nreff; ++ i) {
            rindex[i] -= lines;
        }
        xdf->rindex = rindex.begin();
        xdf->ha = ha.begin();
        xdf->nreff = nreff;
    }
    ~DynamicXDFile()
    {
        /* xdl_free_ctx */
        xdl_cha_free(&xdf->rcha);
    }
    char chg(size_t line)
    {
        return xdf->rchg[line];
    }

private:
    void cha_consume_(long items)
    {
        chastore_t * cha = &xdf->rcha;
        /* xdl_cha_alloc */
        cha->scurr += items * cha->isize;
        while (cha->scurr > cha->head->icurr) {
            auto consumed = cha->head;
            cha->head = consumed->next;
            if (cha->tail == consumed) {
                cha->tail = cha->head;
            }
            if (cha->ancur == consumed) {
                cha->ancur = cha->head;
            }
            cha->scurr -= consumed->icurr;
            xdl_free(consumed);
        }
    }
    void classifier_consume_(xrecord_t * rec)
    {
        /* xdl_classify_record */
        // look up record
        auto* rcrec = cf->rcrecs[rec->ha];

        // update line count
        (pass == 1) ? rcrec->len1-- : rcrec->len2--;
        
        // remove from rhash
        long rhash_hi = (long) XDL_HASHLONG(rec->ha, xdf->hbits);
        auto rhash_it = rhash[(size_t)rhash_hi];
        if (rhash_it == rec) {
            rhash[(size_t)rhash_hi] = rec->next;
        } else {
            while (rhash_it->next != rec) {
                assert(rhash_it->next);
                rhash_it = rhash_it->next;
            }
            rhash_it->next = rec->next;
        }

        // the rcrec is not consumed if unused at this time.
        // another line may still hash to its value.
    }
    xdfile_t * xdf;
    xdlclassifier_t * cf;
    unsigned int pass;
    unsigned int hbits;
    std::vector<xrecord_t*> rhash; // reverse hash lookup, constant size
    boost::container::devector<xrecord_t*> recs; // canonical line list, points into xdf->rcha
    boost::container::devector<char> rchg; // canonical line change list
    boost::container::devector<long> rindex;
    boost::container::devector<unsigned long> ha;

    long mlim;
    boost::container::devector<char> dis;
};

class AsymmetricStreamingXDiff
{
public:
    AsymmetricStreamingXDiff(
        std::string_view old_file,
        unsigned long xdflags = NEED_MINIMAL | IGNORE_CR_AT_EOL /*| HISTOGRAM_DIFF*/ | INDENT_HEURISTIC
    ) : xp{
          .flags = xdflags,
          .ignore_regex = nullptr, .ignore_regex_nr = 0,
          .anchors = nullptr,
          .anchors_nr = 0,
        },
        dynxdfs{
            {
                1,
                &xe.xdf1,
                &cf,
                /* xdl_prepare_env */
                xdl_guess_lines(
                        to_mmfilep<1>(old_file),
                        (XDF_DIFF_ALG(xp.flags) == XDF_HISTOGRAM_DIFF
                         ? XDL_GUESS_NLINES2 : XDL_GUESS_NLINES1)
                )
            }, {
                2,
                &xe.xdf2,
                &cf,
                dynxdfs[0].line_estimate() * 2
            }
        }
    {
        auto line_estimate = dynxdfs[0].line_estimate();

        /* xdl_prepare_env */
        memset(&cf, 0, sizeof(cf));

        mustbe0(xdl_init_classifier(&cf, line_estimate * 2 + 1, (long)xp.flags));

        /* ... */
        dynxdfs[0].extend_pre(&xp, line_estimate, old_file);
        dynxdfs[0].extend_post(false);
    }
    zinc::generator<Diff> diff(zinc::generator<std::string_view> against, ssize_t window_size = -1)
    {
        if (window_size == -1) {
            window_size = std::max({
                    (ssize_t)std::sqrt(dynxdfs[0].size()) + 1,
                    (ssize_t)std::sqrt(dynxdfs[0].line_estimate()) + 1,
                    (ssize_t)7
            });
        }
        auto & f0 = dynxdfs[0];
        if (f0.size()) {
            window.reserve((size_t)(
                        (double)(f0[f0.size() - 1].end() - f0[0].begin()) /
                            (double)f0.size() * (double)window_size * 2
            ));
        }
        size_t l1 = 0, l2 = 0;
        for (auto && new_line : against) {
            extend_env(new_line);
            if ((ssize_t)dynxdfs[1].size() >= window_size) {
                do_diff();                
                for (auto && diff : generate_diffs_for(l1, l2)) {
                    co_yield diff;
                }
                // if l1 or l2 overflows this likely means that file 1 was exhausted while there were still matching values in file 2 for some reason. maybe they weren't passed through nreff?
                while (l1) {
                    dynxdfs[0].consume();
                    -- l1;
                }
                while (l2) {
                    dynxdfs[1].consume();
                    -- l2;
                }
            }
        }
        do_diff();

        while (l1 < dynxdfs[0].size() || l2 < dynxdfs[1].size()) {
            for (auto && diff : generate_diffs_for(l1, l2)) {
                co_yield diff;
            }
        }
    }
    ~AsymmetricStreamingXDiff()
    {
        xdl_free_classifier(&cf);
    }
private:
    zinc::generator<Diff> generate_diffs_for(size_t & l1, size_t & l2)
    {
        bool c1 = (l1 < dynxdfs[0].size()) ? dynxdfs[0].chg(l1) : 0;
        bool c2 = (l2 < dynxdfs[1].size()) ? dynxdfs[1].chg(l2) : 0;
        switch ((c1 << 1) | c2) {
        case (1<<1)|1:
            co_yield Diff(DELETE, xe.xdf1);
            ++ l1;
            co_yield Diff(INSERT, xe.xdf2);
            ++ l2;
            break;
        case (1<<1)|0:
            co_yield Diff(DELETE, xe.xdf1);
            ++ l1;
            break;
        case (0<<1)|1:
            co_yield Diff(INSERT, xe.xdf2);
            ++ l2;
            break;
        case (0<<1)|0:
            assert(xdfile_line(xe.xdf1,0) == xdfile_line(xe.xdf2,0));
            co_yield Diff(EQUAL, xe.xdf1);
            ++ l1;
            ++ l2;
            break;
        }
    }
    //void extend_env(unsigned int lines_estimate, std::span<char> data)
    //{
    //    dynxdfs[1].extend(lines_estimate, data);
    //    // lets update nreff
    //}
    size_t extend_env(std::string_view line) {
        size_t mark_b = window.size();
        //size_t mark_l = dynxdfs[1].size();
        
        assert(line.find('\n') == std::string_view::npos);
        auto window_begin = window.begin();
        window.insert(window.end(), line.begin(), line.end());
        window.push_back('\n');
        if (window.begin() != window_begin) {
            dynxdfs[1].shift_line_ptrs(window.begin() - window_begin);
            window_begin = window.begin();
        }

        dynxdfs[1].extend_pre(
                &xp,
                1,//lines.size(),
                {
                    window.begin() + mark_b, 
                    window.end()
                }
        );
        dynxdfs[1].extend_post(true);
        return dynxdfs[1].size();
    }
    void do_diff()
    {
        auto xe = &this->xe;
        switch (XDF_DIFF_ALG(xp.flags)) {
        case XDF_PATIENCE_DIFF:
            mustbe0(xdl_do_patience_diff(&xp, xe));
            break;
        case XDF_HISTOGRAM_DIFF:
            mustbe0(xdl_do_histogram_diff(&xp, xe));
            break;
        default: {
            /* from xdiff/xdiffi.c . feel free to consolidate. */
#define XDL_MAX_COST_MIN 256
#define XDL_HEUR_MIN_COST 256
#define XDL_SNAKE_CNT 20
        	long ndiags;
        	long *kvdf, *kvdb;
        	xdalgoenv_t xenv;
        	diffdata_t dd1, dd2;

        	/*
        	 * Allocate and setup K vectors to be used by the differential
        	 * algorithm.
        	 *
        	 * One is to store the forward path and one to store the backward path.
        	 */
        	ndiags = xe->xdf1.nreff + xe->xdf2.nreff + 3;
            kvd.resize(2 * (size_t)ndiags + 2);
        	kvdf = kvd.data();
        	kvdb = kvdf + ndiags;
        	kvdf += xe->xdf2.nreff + 1;
        	kvdb += xe->xdf2.nreff + 1;

        	xenv.mxcost = xdl_bogosqrt(ndiags);
        	if (xenv.mxcost < XDL_MAX_COST_MIN)
        		xenv.mxcost = XDL_MAX_COST_MIN;
        	xenv.snake_cnt = XDL_SNAKE_CNT;
        	xenv.heur_min = XDL_HEUR_MIN_COST;

        	dd1.nrec = xe->xdf1.nreff;
        	dd1.ha = xe->xdf1.ha;
        	dd1.rchg = xe->xdf1.rchg;
        	dd1.rindex = xe->xdf1.rindex;
        	dd2.nrec = xe->xdf2.nreff;
        	dd2.ha = xe->xdf2.ha;
        	dd2.rchg = xe->xdf2.rchg;
        	dd2.rindex = xe->xdf2.rindex;

        	mustbe0(xdl_recs_cmp(&dd1, 0, dd1.nrec, &dd2, 0, dd2.nrec,
        		    kvdf, kvdb, (xp.flags & XDF_NEED_MINIMAL) != 0,
        		    &xenv));
        }   break;
        }
    }
    xpparam_t xp;
    xdfenv_t xe;
    boost::container::devector<char> window;
    std::vector<long> kvd; /* stores path vectors in default algorithm */
    DynamicXDFile dynxdfs[2];

    xdlclassifier_t cf;
    std::vector<xdlclass_t*> cf_rchash;
    std::vector<xdlclass_t*> cf_rcrecs;
    std::vector<xrecord_t*> xdf2_rhash;
    std::vector<xrecord_t*> xdf2_recs;
    std::vector<char> xdf2_rchg;
    std::vector<long> xdf2_rindex;
    std::vector<long> xdf2_ha;
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
        mmfile_t o = to_mmfile(data_old), n = to_mmfile(data_new);
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
    			assert(xdfile_line(xe.xdf1,l1)==xdfile_line(xe.xdf2,l2));
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

class AsymmetricStreamingXDiffAdapter {
public:
    AsymmetricStreamingXDiffAdapter() {}

    std::vector<Diff> diff_main(std::string a_, std::string b_, bool)
    {
        std::string a;
        for (auto ch : a_) {
            a.push_back(ch);
            a.push_back('\n');
        }
        std::vector<Diff> result;
        AsymmetricStreamingXDiff xdiff(a);
        auto b = [&]()->zinc::generator<std::string_view> {
            std::string b;
            //b.resize(2);
            //b[1] = '\n';
            b.resize(1);
            for (auto & ch : b_) {
                b[0] = ch;
                co_yield b;
            }
        };
        for (auto && diff : xdiff.diff(b(), (ssize_t)b_.size())) {
            result.emplace_back(std::move(diff));
        }
        return result;
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
  AsymmetricStreamingXDiffAdapter dmp;
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

int main() {
    test_diff_xdiff();
}
//*0

extern "C" const char *__asan_default_options() {
    return
        //"help=1:" // list options on startup
        //"include_if_exists=:" // read more options from the given file/_if it exists
        "check_initialization_order=true:"
        "detect_invalid_pointer_pairs=2:"
        "strict_string_checks=true:"
        "halt_on_error=false:"
        "abort_on_error=true";
}
