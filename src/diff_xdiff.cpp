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
#include <bit>
#include <cassert>
#include <cmath>
#include <iostream> //dbg
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <boost/container/static_vector.hpp>

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

template <typename T>
class queuevector  // a queue with contiguous storage
{
public:
    T* begin() { return std::to_address(vec.begin()) + off; }
    T const* begin() const { return std::to_address(vec.begin()) + off; }
    T* end() { return std::to_address(vec.end()); }
    T const* end() const { return std::to_address(vec.end()); }
    T& operator[](size_t i) { return *(begin() + i); }
    T const& operator[](size_t i) const { return *(begin() + i); }
    T& front() { return vec[off]; }
    T const& front() const { return *begin(); }
    T& back() { return vec[vec.size() - 1]; }
    T const& back() const { return vec[vec.size() - 1]; }
    size_t size() const { return vec.size() - off; }
    bool empty() const { return size() == 0; }
    T* data() { return begin(); }
    T const* data() const { return begin(); }
    void push_back(T const&elem) { insert(end(), &elem, &elem+1); }
    void pop_front() { ++off; }
    size_t capacity_alloc() const { return vec.capacity(); }
    size_t capacity_copy() const { return vec.capacity() - off; }
    void resize(size_t size)
    {
        if (size > vec.capacity()) {
            // new storage is strictly needed
            reserve(std::bit_ceil(size));
        } else if (size + off > vec.capacity()) {
            // changing begin() is still strictly needed
            std::move(begin(), end(), vec.begin());
            off = 0;
        }
        vec.resize(off + size);
    }
    void insert(T* pos, T const* begin, T const*end)
    {
        assert(begin < end);
        size_t insertion_size = (size_t)(end - begin);
        size_t new_size = insertion_size + size();
        if (new_size > vec.capacity()) {
            // new storage is strictly needed
            std::vector<T> vec2;
            vec2.reserve(std::bit_ceil(new_size));

            vec2.insert(vec2.end(), std::make_move_iterator(this->begin()), std::make_move_iterator(pos));
            vec2.insert(vec2.end(), begin, end);
            vec2.insert(vec2.end(), std::make_move_iterator(pos), std::make_move_iterator(this->end()));

            vec = std::move(vec2);
            off = 0;
        } else if (
                new_size + off > vec.capacity()
        ) {
            // adjusting begin() is still strictly needed
            T* posend = pos + insertion_size;
            T* adjpos = pos - off;
            T* adjposend = posend - off;
            std::move(this->begin(), pos, vec.begin());
            if (adjposend < pos) {
                std::copy(begin, end, adjpos);
                // this move is not needed if off is clamped to size or something
                std::move(posend, this->end(), adjposend);
            } else if (adjposend == pos) {
                std::copy(begin, end, adjpos);
            } else if (adjposend > posend) {
                T const* mid = begin + (posend - adjpos);
                std::copy(begin, mid, adjpos);
                vec.insert(typename std::vector<T>::iterator(posend), mid, end);
                assert(!"can you review this branch for correctness");
            }
            off = 0;
        } else {
            // could check here if user would want to adjust off instead of size
            vec.insert(typename std::vector<T>::iterator(pos), begin, end);
        }
    }
    void erase(T* begin, T* end) {
        if (begin - this->begin() < this->end() - end) {
            // less data is copied if off is adjusted than if size is
            std::move_backward(this->begin(), begin, end);
            off = (size_t)(end + off - begin); // off += end - begin
        } else {
            // less data is copied if size is adjusted than if off is
            vec.erase(typename std::vector<T>::iterator(begin), typename std::vector<T>::iterator(end));
        }
    }
    void reserve(size_t size) {
        if (size > vec.capacity()) {
            std::vector<T> vec2;
            vec2.reserve(size);
            vec2.insert(vec2.end(), std::make_move_iterator(begin()), std::make_move_iterator(end()));
            vec = std::move(vec2);
            off = 0;
        }
    }
private:
    std::vector<T> vec;
    size_t off;
};

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

struct classifier_data
{
    xdlclassifier_t cf;
    std::vector<xdlclass_t*> consumed_rchash; // references to consumed memory are still kept for their line counts but no longer have valid buffers to compare
    operator xdlclassifier_t&() { return cf; }
    operator xdlclassifier_t*() { return &cf; }
    auto operator&() { return &cf;}
    auto operator->() { return &cf;}
};

class DynamicXDFile
{
public:
    DynamicXDFile(unsigned int pass, xdfile_t * xdf, classifier_data & cfd, long line_estimate)
    : xdf(xdf), cf(cfd), pass(pass)
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
            auto rcrec = cf->rcrecs[rec->ha];
            if (rcrec->line == rec->ptr) {
                rcrec->line += offset;
            }
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

        if (narec * 2 <= cf->hsize) {
            // notably the classifier hash table is not yet grown
            ///*dbg*/assert(narec * 2 < cf->hsize && "line estimate was too short by a factor of at least 2");

            /*
             * notes on classifier state
             * - hsize appears to hold the size of cf->rchash
             * - xdl_init_classifier inits hbits and hsize based on the passed line count
             * - it also allocates cf->rchash to hsize
             * hsize is not otherwise used in xprepare that i see
             * hbits is used to calculate the hash idx when rec is classified
             * so if hbits is changed we could:
             * - resize cf->rchash
             * - reseat everything into cf->rchash
             *      - each rcrec->ha inside cf->rcrecs up to cf->count
             *        has the ->ha used to make the rchash index
             */
            cf->hbits = xdl_hashbits((unsigned int) narec * 4);
            cf->hsize = 1 << cf->hbits;
            xdl_free(cf->rchash);
            XDL_CALLOC_ARRAY(cf->rchash, (size_t)cf->hsize);
            cf.consumed_rchash.clear();
            cf.consumed_rchash.resize((size_t)cf->hsize);
            for (auto it = cf->rcrecs; it != &cf->rcrecs[cf->count]; ++ it) {
                auto rcrec = *it;
                long hi = (long) XDL_HASHLONG(rcrec->ha, cf->hbits);
                if (rcrec->line) {
                    rcrec->next = cf->rchash[hi];
                    cf->rchash[hi] = rcrec;
                } else {
                    rcrec->next = cf.consumed_rchash[(size_t)hi];
                    cf.consumed_rchash[(size_t)hi] = rcrec;
                }
            }
        }

        if (narec >= rhash.size()) {
            /*
             * note: i'm actually not seeing that rhash is used anywhere.
             */
            hbits = xdl_hashbits((unsigned int) narec);
            rhash.clear(); // ensure fill with nullptr
            rhash.resize(1 << hbits);
            for (auto rec : recs) {
                size_t hi = XDL_HASHLONG(rec->ha, hbits);
                rhash[hi] = rec;
            }
        }

        recs.resize(narec);

        nrec = xdf->nrec;
        for (cur = &*data.begin(), top = &*data.end(); cur < top; ) {
            prev = cur;
            hav = xdl_hash_record(&cur, top, (long)xpp->flags);
            recs.resize(narec = (unsigned int)nrec + 1);
            cantbe0(crec = xdl_cha_alloc(&xdf->rcha));
            crec->ptr = prev;
            crec->size = (long) (cur - prev);
            crec->ha = hav;
            recs[(size_t)nrec++] = crec;

            /* if the classifier entry hash been consumed, recover it, then classify the line. otherwise, classify the line, then update the line pointer in case old lines are consumed. */
            auto rchash_hi = (long) XDL_HASHLONG(crec->ha, cf->hbits);
            xdlclass_t ** prev;
            xdlclass_t * rcrec;
            for (
                prev = &cf.consumed_rchash[(size_t)rchash_hi];
                (rcrec = *prev);
                prev = &(*prev)->next
            ) {
                if (rcrec->ha == crec->ha && rcrec->size == crec->size) {
                    break;
                };
            }
            if (rcrec) {
                assert(rcrec->next == nullptr && "hash collision in consumed lines; should check for existing matching entry before using consumed entry");
                *prev = rcrec->next;

                rcrec->line = crec->ptr;

                rcrec->next = cf->rchash[rchash_hi];
                cf->rchash[rchash_hi] = rcrec;

                mustbe0(xdl_classify_record(pass, cf, rhash.data(), hbits, crec));
            } else {

                mustbe0(xdl_classify_record(pass, cf, rhash.data(), hbits, crec));

                /* ensure the line pointer points to the latest line data so old data can be deallocated */
                rcrec = cf->rcrecs[crec->ha];
                assert(rcrec->ha == hav);
                assert(std::string_view(rcrec->line, (size_t)rcrec->size) == std::string_view(crec->ptr, (size_t)crec->size));
                rcrec->line = crec->ptr;
            }
        }

        rchg.resize((size_t)nrec + 2);
	    if ((XDF_DIFF_ALG(xpp->flags) != XDF_PATIENCE_DIFF) &&
    	    (XDF_DIFF_ALG(xpp->flags) != XDF_HISTOGRAM_DIFF)) {
            rindex.resize((size_t)nrec + 1);
            ha.resize((size_t)nrec + 1);
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
    void extend_post(xpparam_t * xpp, bool compare_nreff)
    {
	    if ((XDF_DIFF_ALG(xpp->flags) != XDF_PATIENCE_DIFF) &&
            (XDF_DIFF_ALG(xpp->flags) != XDF_HISTOGRAM_DIFF)) {

            /* ~~ xdl_optimize_ctxs */
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
                dis.resize((size_t)(xdf->dend + 1));

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
        for (long i = 0 ; i < lines; ++ i) {
            classifier_consume_(recs[(size_t)i]);
        }
        for (auto idx = 0 ; idx < lines; ++ idx) {
            std::cerr << "consuming rec ptr " << (void*)recs[(size_t)idx]->ptr << std::endl;
        }
        recs.erase(recs.begin(), recs.begin() + lines);
        xdf->recs = recs.begin();
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
        rindex.erase(rindex.begin(), rindex.begin() + nreff_off);
        // uhh does this get vectorized? or is it too in-place? what are the right codelines here?
        for (size_t i = 0; i < (size_t)nreff; ++ i) {
            rindex[i] -= lines;
        }
        rchg.erase(rchg.begin(), rchg.begin() + nreff_off);
        xdf->rchg = rchg.begin() + 1;
        ha.erase(ha.begin(), ha.begin() + nreff_off);
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

    void trace_state(std::string_view context) const
    {
        std::cerr << "Tracing state for context: " << context << std::endl;
        std::cerr << "recs:" << std::endl;
        for (size_t i = 0; i < recs.size(); ++i) {
            std::cerr << "  rec[" << i << "]: ";
            if (recs[i]) {
                std::cerr << std::string_view(recs[i]->ptr, (size_t)recs[i]->size) << std::endl;
            } else {
                std::cerr << recs[i] << std::endl;
            }
        }
        std::cerr << "rindex:" << std::endl;
        for (size_t i = 0; i < rindex.size(); ++i) {
            std::cerr << "  rindex[" << i << "]: " << rindex[i] << std::endl;
        }
        std::cerr << "rchg:" << std::endl;
        for (auto *p = xdf->rchg; p < rchg.end(); ++p) {
            std::cerr << "  rchg[" << (p-xdf->rchg) << "]: " << static_cast<int>(*p) << std::endl;
        }
        std::cerr << "nrec: " << xdf->nrec << ", nreff: " << xdf->nreff << std::endl;
        std::cerr << "dstart: " << xdf->dstart << ", dend: " << xdf->dend << std::endl;
        std::cerr << "----------------------------------------" << std::endl;
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
        long rhash_hi = (long) XDL_HASHLONG(rec->ha, xdf->hbits);

        // look up record
        auto* rcrec = cf->rcrecs[rec->ha];
        assert(std::string_view(rcrec->line, (size_t)rcrec->size) == std::string_view(rec->ptr, (size_t)rec->size));

        // line count not decremented in order to keep whole-file data accurate for heuristics
        //// update line count
        //(pass == 1) ? rcrec->len1-- : rcrec->len2--;

        // if the class record is using this line's buffer, change it
        if (rcrec->line == rec->ptr) {
            xrecord_t *it_rec;
            for (
                xrecord_t **rhash_it = &rhash[(size_t)rhash_hi];
                (it_rec = *rhash_it);
                rhash_it = &it_rec->next
            ) {
                if (it_rec != rec && it_rec->ha == rec->ha) {
                    break;
                }
            }
            if (it_rec) {
                // another line is holding this data -- update the consumed buffer to point to it
                assert(it_rec->size == rec->size);
                assert(std::string_view(it_rec->ptr, (size_t)it_rec->size) == std::string_view(rec->ptr, (size_t)rec->size));
                rcrec->line = it_rec->ptr;
            } else {
                // no other line is holding this data -- move the consumed record to a stash
                long rchash_hi = (long) XDL_HASHLONG(rcrec->ha, cf->hbits);
                xdlclass_t ** prev;
                for (
                    prev = &cf->rchash[rchash_hi];
                    *prev != rcrec;
                    prev = &(*prev)->next
                ) {}
                *prev = rcrec->next;
                rcrec->line = nullptr;
                rcrec->next = cf.consumed_rchash[(size_t)rchash_hi];
                cf.consumed_rchash[(size_t)rchash_hi] = rcrec;
            }
        }

        // remove from rhash
        xrecord_t **prev;
        for (
            prev = &rhash[(size_t)rhash_hi];
            *prev != rec;
            prev = &(*prev)->next
        ) { }
        *prev = rec->next;
    }
    xdfile_t * xdf;
    classifier_data & cf;
    unsigned int pass;
    unsigned int hbits;
    std::vector<xrecord_t*> rhash; // reverse hash lookup, constant size
    queuevector<xrecord_t*> recs; // canonical line list, points into xdf->rcha
    queuevector<char> rchg; // canonical line change list
    queuevector<long> rindex;
    queuevector<unsigned long> ha;

    long mlim;
    queuevector<char> dis;
};

class AsymmetricStreamingXDiff
{
public:
    AsymmetricStreamingXDiff(
        std::string_view old_file,
        ssize_t window_size = -1,
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
                cf,
                /* xdl_prepare_env */
                xdl_guess_lines(
                        to_mmfilep<1>(old_file),
                        (XDF_DIFF_ALG(xp.flags) == XDF_HISTOGRAM_DIFF
                         ? XDL_GUESS_NLINES2 : XDL_GUESS_NLINES1)
                )
            }, {
                2,
                &xe.xdf2,
                cf,
                dynxdfs[0].line_estimate() > window_size
                    ? dynxdfs[0].line_estimate() * 2
                    : window_size * 4
            }
        }
    {
        /* xdl_prepare_env */
        memset(&cf.cf, 0, sizeof(cf.cf));

        mustbe0(xdl_init_classifier(&cf, dynxdfs[0].line_estimate() + dynxdfs[1].line_estimate() + 1, (long)xp.flags));
        cf.consumed_rchash.clear();
        cf.consumed_rchash.resize((size_t)cf->hsize);

        /* ... */
        dynxdfs[0].extend_pre(&xp, dynxdfs[0].line_estimate(), old_file);
        dynxdfs[0].extend_post(&xp, false);

        dynxdfs[1].extend_pre(&xp, 0, {});
        dynxdfs[1].extend_post(&xp, false);

        if (window_size < 0) {
            window_size = std::max({
                    (ssize_t)std::sqrt(dynxdfs[0].size()) + 1,
                    (ssize_t)std::sqrt(dynxdfs[0].line_estimate()) + 1,
                    (ssize_t)7
            });
        }
        this->window_size = (size_t)window_size;
    }
    zinc::generator<Diff> diff(zinc::generator<std::string_view> against)
    {
        //auto & f0 = dynxdfs[0];
        //if (f0.size()) {
        //    window.reserve_back((size_t)(
        //            (double)(f0[f0.size() - 1].end() - f0[0].begin()) /*bytes size of file 1*/ /
        //                (double)f0.size() /*lines size of file 1*/ * (double) window_size * 2
        //    ));
        //}/*dbg uncomment*/
        size_t l1 = 0, l2 = 0;
        size_t tot2 = 0;
        for (auto && new_line : against) {
            extend_env(new_line);
            if (dynxdfs[1].size() >= window_size) {
                do_diff();
                co_yield get_diff_for(l1, l2);
                // if l1 or l2 overflows this likely means that file 1 was exhausted while there were still matching values in file 2 for some reason. maybe they weren't passed through nreff?
                while (l1) {
                    dynxdfs[0].consume();
                    -- l1;
                }
                tot2 += l2;
                while (l2) {
                    dynxdfs[1].consume();
                    -- l2;
                }
                /*dbg: consume window*/
            }
        }
        do_diff();

        while (l1 < dynxdfs[0].size() || l2 < dynxdfs[1].size()) {
            co_yield get_diff_for(l1, l2);
        }
    }
    ~AsymmetricStreamingXDiff()
    {
        xdl_free_classifier(&cf);
    }
private:
    Diff get_diff_for(size_t & l1, size_t & l2)
    {
        if (l1 < dynxdfs[0].size()) {
            if (dynxdfs[0].chg(l1)) {
                return {DELETE, xe.xdf1, l1 ++};
            }
        }
        if (l2 < dynxdfs[1].size()) {
            if (dynxdfs[1].chg(l2)) {
                return {INSERT, xe.xdf2, l2 ++};
            }
        }
        assert(l1 < dynxdfs[0].size() && l2 < dynxdfs[1].size());
        assert(xdfile_line(xe.xdf1,l1) == xdfile_line(xe.xdf2,l2));
        return {EQUAL, xe.xdf2, (l1 ++, l2 ++)};
    }
    void assert_no_dangling_pointers() {
        // Macro to assert pointer validity, with dynamic error message using variable name
        #define ASSERT_POINTER_VALID(ptr, store1, store2) \
            assert(((ptr) >= (store1).data() && (ptr) < (store1).data() + (store1).size()) || \
                   ((ptr) >= (store2).data() && (ptr) < (store2).data() + (store2).size()) || \
                   ! #ptr " is invalid")

        // Helper lambda to iterate over a linked list and perform a custom action
        auto for_each_in_linked_list = [](xdlclass_t* head, auto&& action) {
            for (xdlclass_t* node = head; node; node = node->next) {
                action(node);
            }
        };

        // Helper lambda to check if a record exists in a linked list
        auto linked_list_contains = [&](xdlclass_t* head, xdlclass_t* target) -> bool {
            bool found = false;
            for_each_in_linked_list(head, [&](xdlclass_t* node) {
                if (node == target) {
                    found = true;
                }
            });
            return found;
        };

        const auto& window = this->window;
        const std::string_view old_file = xe.xdf1.nrec ? std::string_view(xe.xdf1.recs[0]->ptr, xe.xdf1.recs[xe.xdf1.nrec-1]->ptr+xe.xdf1.recs[xe.xdf1.nrec-1]->size) : std::string_view();


        // Iterate over both dynxdfs
        for (size_t xdfi = 0; xdfi < 2; ++ xdfi) {
            auto xdf = xdfi ? &xe.xdf2 : &xe.xdf1;
            // Check recs pointers
            for (xrecord_t** it = xdf->recs; it != xdf->recs + xdf->nrec; ++ it) {
                auto rec = *it;
                ASSERT_POINTER_VALID(rec->ptr, window, old_file);
            }
        }
    
        // Iterate over classifier_data
        // Check rcrecs array
        for (long i = 0; i < cf->count; ++i) {
            xdlclass_t* rcrec = cf->rcrecs[i];
            if (rcrec) {
                // Check if the record is in rchash or consumed_rchash
                bool in_rchash = std::any_of(cf->rchash, cf->rchash + cf->hsize, [&](xdlclass_t* head) {
                    return linked_list_contains(head, rcrec);
                });
    
                if (in_rchash) {
                    ASSERT_POINTER_VALID(rcrec->line, window, old_file);
                } else {
                    // Ensure the record is in consumed_rchash with line == nullptr
                    bool in_consumed = std::any_of(cf.consumed_rchash.begin(), cf.consumed_rchash.end(), [&](xdlclass_t* head) {
                        return linked_list_contains(head, rcrec);
                    });
                    assert(in_consumed && "rcrec not in rchash or consumed_rchash");
                    assert(rcrec->line == nullptr && "rcrec in consumed_rchash has line != nullptr");
                }
            }
        }
    
        for (long hi = 0; hi < cf->hsize; ++hi) {
            for_each_in_linked_list(cf->rchash[hi], [&](xdlclass_t* node) {
                ASSERT_POINTER_VALID(node->line, window, old_file);
            });
        }
        for (size_t hi = 0; hi < cf.consumed_rchash.size(); ++hi) {
            for_each_in_linked_list(cf.consumed_rchash[hi], [&](xdlclass_t* node) {
                assert(node->line == nullptr && "Line pointer in consumed_rchash is not null");
            });
        }
        #undef ASSERT_POINTER_VALID
    }
    size_t extend_env(std::string_view line) {
        size_t mark_b = window.size();
        //size_t mark_l = dynxdfs[1].size();
        
        assert(line.find('\n') == std::string_view::npos);
        assert_no_dangling_pointers();
        auto window_begin = window.begin();
        auto window_capacity = window.capacity_copy();
        window.insert(window.end(), line.begin(), line.end());
        window.push_back('\n');
        // dbg it doesn't look like window is being consumed
        if (window.begin() != window_begin) {
            std::cerr << "WINDOW RESIZE" << window_capacity << "=>" << window.capacity_copy() << std::endl;
            dynxdfs[1].shift_line_ptrs(window.begin() - window_begin);
            assert_no_dangling_pointers();
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
        dynxdfs[1].extend_post(&xp, true);
        return dynxdfs[1].size();
    }
    void do_diff()
    {
        auto xe = &this->xe;
        // for now rchg is wiped as the git/xdiff algorithms assume this
        std::memset(xe->xdf1.rchg - 1, 0, (size_t)(xe->xdf1.nrec + 2) * sizeof(*xe->xdf1.rchg));
        std::memset(xe->xdf2.rchg - 1, 0, (size_t)(xe->xdf2.nrec + 2) * sizeof(*xe->xdf2.rchg));
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
    size_t window_size;
    queuevector<char> window;
    std::vector<long> kvd; /* stores path vectors in default algorithm */
    DynamicXDFile dynxdfs[2];

    classifier_data cf;
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

        static thread_local std::array<char,1024*16> storage;
        auto storage_end = storage.begin();

        AsymmetricStreamingXDiff xdiff(a, (ssize_t)b_.size() / 2, NEED_MINIMAL | IGNORE_CR_AT_EOL /*| HISTOGRAM_DIFF*/);
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
        for (auto && diff : xdiff.diff(b())) {

            std::copy(diff.text.begin(), diff.text.end(), storage_end);
            std::string_view text(storage_end, storage_end + diff.text.size());
            storage_end += diff.text.size();

            if (!result.size()) {
                result.emplace_back(diff.type, text);
            } else if (result.back().type == diff.type) {
                // reunify lines into a string
                assert(result.back().text.end() == text.begin());
                result.back() = Diff(diff.type, std::string_view(result.back().text.begin(), text.end()));
            } else {
                result.emplace_back(diff.type, text);
            }
        }
        return result;
    }
};

void assertEquals(char const*desc, std::vector<Diff>&expected, std::vector<Diff>const&actual)
{
    std::cerr << desc << std::endl;
    /*dbg*/static std::vector<Diff> alternative{Diff(INSERT, "x"), Diff(EQUAL, "a"), Diff(INSERT, "xcxa"), Diff(EQUAL, "bc"), Diff(DELETE, "y")};
    assert(std::equal(expected.begin(),expected.end(),actual.begin(),actual.end()) || std::equal(alternative.begin(),alternative.end(),actual.begin(),actual.end()));
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

#define __COMMON_SAN_DEFAULT_OPTIONS   \
        /* list options on startup     \
        "help=1" ":" */                \
        /* read more options from file \
        "include_if_exists=" ":" */    \
        "print_stacktrace=true"        \
        ":"                            \
        "report_error_type=true"       \
        ":"                            \
        "strict_string_checks=true"    \
        ":"                            \
        "abort_on_error=true"          \
        ":"                            \
        "halt_on_error=true"

extern "C" char const*__asan_default_options()
{
    return __COMMON_SAN_DEFAULT_OPTIONS
        ":check_initialization_order=true"
        ":detect_invalid_pointer_pairs=2"
    ;
}
extern "C" char const*__ubsan_default_options()
{
    return __COMMON_SAN_DEFAULT_OPTIONS
    ;
}
