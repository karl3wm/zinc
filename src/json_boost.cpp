#include <zinc/json.hpp>

#include <boost/container/devector.hpp>
#include <boost/json/basic_parser_impl.hpp>
#define enum public: enum // make serializer public in boost 1.81.0
#include <boost/json/serializer.hpp>
#undef enum
#include <boost/json/impl/serializer.ipp>

#include <map>
#include <memory>
//#include <iostream> /*dbg*/

using namespace boost::json;

namespace zinc {

namespace {

struct ParseHandler
{
    static constexpr std::size_t max_object_size = (size_t)-1;
    static constexpr std::size_t max_array_size = (size_t)-1;
    static constexpr std::size_t max_key_size = (size_t)-1;
    static constexpr std::size_t max_string_size = (size_t)-1;

    ///*
    void dbg_walk(JSON const*json, int depth=0) {
        static JSON dbgval;
        dbgval = *json;
        if (auto*array = std::get_if<std::span<JSON>>(json)) {
            //if(depth==0)std::cerr << "array "; /*dbg*/
            assert((void*)&*array->begin() >= (void*)store.begin() && (void*)&*array->end() <= (void*)store.end() && array->end() >= array->begin());
            for (auto & elem : *array) {
                dbg_walk(&elem, depth+1);
            }
        } else if(auto*dict = std::get_if<std::span<KeyJSONPair>>(json)) {
            //if(depth==0)std::cerr << "dict "; /*dbg*/
            assert((void*)&*dict->begin() >= (void*)&*store.begin() && (void*)&*dict->end() <= (void*)&*store.end() && dict->end() >= dict->begin());
            for (auto & [k,v] : *dict) {
                assert(k.begin() >= (void*)&*store.begin() && (void*)&*k.end() <= (void*)&*store.end() && k.end() >= k.begin());
                static std::string string;
                string = k;
                dbg_walk(&v, depth+1);
            }
        } else if(auto*str = std::get_if<std::string_view>(json)) {
            assert((!str->size() && str->begin() == nullptr) || ((void*)&*str->begin() >= (void*)&*store.begin() && (void*)&*str->end() <= (void*)&*store.end() && str->end() >= str->begin()));
            //if(depth==0)std::cerr << "\"" << *str << "\" "; /*dbg*/
            static std::string string;
            string = *str;
        }
    }
    //*/

    struct DocImpl : public zinc::JSON {
        DocImpl(
            JSON root, ParseHandler* handler,
            char* store_start, char* store_end,
            JSON** ptrs_start, JSON** ptrs_end
        ) : JSON(root), handler(handler),
            store_start(store_start), store_end(store_end),
            ptrs_start(ptrs_start), ptrs_end(ptrs_end)
        { }
        ParseHandler * handler;
        char* store_start;
        char* store_end;
        JSON** ptrs_start;
        JSON** ptrs_end;
    };
    DocImpl* docimpl;

    std::vector<JSON> stack;
    boost::container::devector<char> store;
    boost::container::devector<JSON*> ptr_jsons;
    boost::container::devector<DocImpl*> docs;

    JSON* get() {
        auto ptr = docimpl;
        docimpl = nullptr;
        return ptr;
    }
    void abort() {
        assert(docs.back() == docimpl);
        store.erase(docimpl->store_start, store.end());
        ptr_jsons.erase(docimpl->ptrs_start, ptr_jsons.end());
        docs.pop_back();
        docimpl = nullptr;
    }
    void del(DocImpl& doc) {
        assert((store.end() == doc.store_end || store.begin() == doc.store_start) && "deallocate jsons in forward or reverse order");
        assert((ptr_jsons.end() == doc.ptrs_end || ptr_jsons.begin() == doc.ptrs_start) && "deallocate jsons in forward or reverse order");
        decltype(docs)::iterator doc_it;
        if (docs.back() == &doc) {
            doc_it = docs.end() - 1;
        } else if (docs.front() == &doc) {
            doc_it = docs.begin();
        } else {
            assert(!"deallocate jsons in forward or reverse order");
        }
        assert(doc.store_start >= store.begin() && doc.store_end <= store.end());
        assert(doc.store_start <= doc.store_end);
        assert(doc.ptrs_start >= ptr_jsons.begin() && doc.ptrs_end <= ptr_jsons.end());
        assert(doc.ptrs_start <= doc.ptrs_end);
        store.erase(doc.store_start, doc.store_end);
        ptr_jsons.erase(doc.ptrs_start, doc.ptrs_end);
        docs.erase(doc_it);
    }
    static void del(JSON* doc_) {
        DocImpl & doc = *(DocImpl*)doc_;
        doc.handler->del(doc);
    }

    inline void adjust_store_json_ptrs(JSON*json, ptrdiff_t store_offset)
    {
        if (auto span = std::get_if<std::span<JSON>>(json)) {
            *span = {
                (JSON*)((char*)span->data() + store_offset),
                span->size()
            };
        } else if (auto span = std::get_if<std::span<KeyJSONPair>>(json)) {
            *span = {
                (KeyJSONPair*)((char*)span->data() + store_offset),
                span->size()
            };
            for (auto & [k,_] : *span) {
                k = {
                    k.data() + store_offset,
                    k.size()
                };
            }
        } else if (auto sv = std::get_if<std::string_view>(json)) {
            *sv = {
                sv->data() + store_offset,
                sv->size()
            };
            static std::string dbgstr;
            dbgstr = std::get<std::string_view>(*json);
        } else {
            throw std::logic_error("non-pointer json in ptr_jsons");
        }
    }
    inline void adjust_store_pointers(ptrdiff_t store_offset)
    {
        if (!store_offset) {
            return;
        }
        for (JSON & json : stack) {
            if (json_is_ptr(json, store_offset)) {
                adjust_store_json_ptrs(&json, store_offset);
            }
        }
        for (JSON *& json : ptr_jsons) {
            json = (JSON*)((char*)json + store_offset);
            adjust_store_json_ptrs(json, store_offset);
        }   
        for (DocImpl*& doc : docs) {
            doc = (DocImpl*)((char*)doc + store_offset);
            assert(doc->handler == this);
            doc->store_start += store_offset;
            doc->store_end += store_offset;
            assert(doc->store_start >= store.begin() && doc->store_end <= store.end());
            assert(doc->store_start <= doc->store_end);
        }
        if (docimpl != nullptr) {
            docimpl = (DocImpl*)((char*)docimpl + store_offset);
        }
    }

    inline void adjust_ptr_pointers(ptrdiff_t ptrs_offset)
    {
        if (!ptrs_offset) {
            return;
        }
        for (DocImpl*& doc : docs) {
            assert(doc->handler == this);
            doc->ptrs_start = (JSON**)((char*)doc->ptrs_start + ptrs_offset);
            doc->ptrs_end = (JSON**)((char*)doc->ptrs_end + ptrs_offset);
            assert(doc->ptrs_start >= ptr_jsons.begin() && doc->ptrs_end <= ptr_jsons.end());
            assert(doc->ptrs_start <= doc->ptrs_end);
        }
    }

    inline char* reserve(size_t n, size_t a  = 1) {
        ///*
        for (auto &json : stack) {
            dbg_walk(&json);
        }
        //*/
        size_t avail;
        void* result;
        result = std::align(a, n, result = store.end(), avail = store.back_free_capacity());
        if (!result) {
            size_t min_capacity = n + store.size() + a;
            size_t new_capacity = std::max(avail + store.size(), 64ul);
            while (new_capacity < min_capacity) {
                new_capacity *= 2;
            }
            char* old = store.data();
            store.reserve_back(new_capacity);
            adjust_store_pointers(store.data() - old);
            ///*
            for (auto &json : stack) {
                dbg_walk(&json);
            }
            //*/
            result = std::align(a, n, result = store.end(), avail = store.back_free_capacity());
            assert(result);
        }
        if (result > store.end()) {
            store.resize((size_t)((char*)result - store.begin()));
        }
        return (char*)result;
    }
    inline char* store_chars(std::string_view chars)
    {
        reserve(chars.size());
        store.insert(store.end(), chars.begin(), chars.end());
        return store.end() - chars.size();
    }
    inline std::string_view& store_key(std::string_view key)
    {
        //auto seat = (std::string_view*)reserve(sizeof(key));
        auto dbgcmp1 = store.end(); /* checking reserve is a no-op because key is only used in contiguous spans of pairs */
        auto dbgcmp2 = store.insert(reserve(sizeof(key), alignof(decltype(key))), (char*)&key, (char*)(&key+1));
        assert(dbgcmp1 == dbgcmp2);
        //store.resize_back(store.size() + sizeof(key));
        //new (seat) std::string_view(key);
        return *(std::string_view*)(store.end() - sizeof(key));
    }
    bool json_is_ptr(JSON const & json, ptrdiff_t store_offset = 0) {
        if (
            std::holds_alternative<std::span<JSON>>(json) ||
            std::holds_alternative<std::span<KeyJSONPair>>(json)
        ) {
            return true;
        } else if (std::string_view const * sv = std::get_if<std::string_view>(&json)) {
            if (sv->data() >= store.data() - store_offset
             && sv->data() <= &*store.end() - store_offset) {
                assert(sv->size() + sv->data() <= &*store.end());
                return true;
            }
        }
        return false;
    }
    void add_ptr_json(JSON* json)
    {
        char* old = (char*)ptr_jsons.data();
        ptr_jsons.push_back(json);
        if ((char*)ptr_jsons.data() != old) {
            adjust_ptr_pointers((char*)ptr_jsons.data() - old);
        }
    }
    inline JSON& store_json(JSON const & json)
    {
        bool is_ptr = json_is_ptr(json);
        ///*
        dbg_walk(&json);
        for (auto &json : stack) {
            dbg_walk(&json);
        }
        //*/

        auto seat = (JSON*)reserve(sizeof(JSON), alignof(JSON));
        dbg_walk(&json);
        auto dbg = store.begin();
        store.insert(store.end(), (char*)&json, (char*)(&json + 1));
        assert(store.begin() == dbg);
        //store.resize_back(store.size() + sizeof(json));
        //new (seat) JSON(json);
        dbg_walk(seat);

        if (is_ptr) {
            add_ptr_json(seat);
        }

        return *seat;//store.end();
    }

    inline DocImpl& store_doc()
    {
        //std::cerr<<"store_doc start"<<std::endl;
        assert(nullptr == docimpl);
        char* storeptr = reserve(sizeof(DocImpl), alignof(DocImpl));
        JSON** ptrsptr = ptr_jsons.end();
        docimpl = (DocImpl*)storeptr;
        docs.push_back(docimpl);
        store.resize_back(store.size() + sizeof(DocImpl));
        new (docimpl) DocImpl(
            nullptr, this,
            storeptr, storeptr,
            ptrsptr, ptrsptr
        );
        return *docimpl;//store.end();
    }

    //inline void set_raw(std::string_view raw) {
    //    JSON & json = stack.back();
    //    assert(json.raw.empty());
    //    json.raw = raw;
    //}

    inline bool on_document_begin(error_code&) {
        //std::cerr<<"on_document_begin"<<std::endl;
        store_doc();
        return true;
    }
    inline bool on_document_end(error_code&) {
        assert(1 == stack.size());
        assert(docimpl->handler == this);
        *(JSON*)docimpl = stack.back();
        stack.clear();
        if (json_is_ptr(*docimpl)) {
            add_ptr_json(docimpl);
        }
        docimpl->store_end = store.end();
        docimpl->ptrs_end = ptr_jsons.end();
        return true;
    }
    inline bool on_object_begin(error_code&) {
        //std::cerr<< "ON_OBJECT_BEGIN" << std::endl; /*dbg*/
        return true;
    }
    inline bool on_object_end(std::size_t n, error_code&) {
        //std::cerr<< "ON_OBJECT_END start" << std::endl; /*dbg*/
        ///*
        for (auto &json : stack) {
            dbg_walk(&json);
        }
        //*/
        auto store_start = (KeyJSONPair*)reserve(sizeof(KeyJSONPair) * n + sizeof(JSON), alignof(KeyJSONPair));
        auto dbg_store_start = store.begin();
        auto stack_end = stack.end(), stack_start = stack_end - (ssize_t)n * 2;
        for (auto it = stack_start; it != stack_end;) {
            // this shouldn't cause reallocation because reserve was called earlier
            store_key(std::get<std::string_view>(*it));
            ++ it;
            store_json(*it);
            ++ it;
        }
        assert(store.begin() == dbg_store_start);
        auto store_end = (KeyJSONPair*)&*store.end();
        assert(store_end == store_start + n);
        stack.erase(stack_start, stack_end);
        stack.emplace_back(std::span<KeyJSONPair>(store_start, store_end));
        //std::cerr<< "ON_OBJECT_END end" << std::endl; /*dbg*/
        return true;
    }
    inline bool on_array_begin(error_code&)
    {
        return true;
    }
    inline bool on_array_end(std::size_t n, error_code&)
    {
        auto store_start = (JSON*)reserve(sizeof(JSON) * n + sizeof(JSON), alignof(JSON));
        auto stack_end = stack.end(), stack_start = stack_end - (ssize_t)n;
        for (auto it = stack_start; it != stack_end; ++ it) {
            store_json(*it);
        }
        auto store_end = (JSON*)&*store.end();
        stack.erase(stack_start, stack_end);
        stack.emplace_back(std::span<JSON>(store_start, store_end));
        return true;
    }
    /*
     * boost does not forward the original document ranges to on_key* and
     * on_part*. rather, it puts the data in a buffer on the stack, and hands
     * pointers that point into this buffer.
     */
    inline bool on_key_part(string_view s, std::size_t, error_code&) {
        store_chars(s);
        return true;
    }
    inline bool on_key(string_view s, std::size_t n, error_code&) {
        /* if boost::json were adjusted or if the raw pointer offsets were tracked
         * then the original data could be referenced directly here.
         * escaping can possibly be checked for with s.size() != n.
         */
        store_chars(s);
        auto end = store.end(), start = end - n;
        stack.emplace_back(std::string_view{start, end});
        return true;
    }
    inline bool on_string_part(string_view s, std::size_t, error_code&) {
        store_chars(s);
        return true;
    }
    inline bool on_string(string_view s, std::size_t n, error_code&) {
        /* if boost::json were adjusted or if the raw pointer offsets were tracked
         * then the original data could be referenced directly here.
         * escaping can possibly be checked for with s.size() != n.
         */
        store_chars(s);
        auto end = store.end(), start = end - n;
        stack.emplace_back(std::string_view{start, end});
        return true;
    }
    inline bool on_number_part(string_view, error_code&)
    {
        return true;
    }
    inline bool on_int64(std::int64_t i, string_view, error_code&)
    {
        stack.emplace_back((long)i);
        return true;
    }
    inline bool on_uint64(std::uint64_t u, string_view, error_code&)
    {
        stack.emplace_back((long)u);
        return true;
    }
    inline bool on_double(double d, string_view, error_code&)
    {
        stack.emplace_back(d);
        return true;
    }
    inline bool on_bool(bool b, error_code&)
    {
        stack.emplace_back(b);
        return true;
    }
    inline bool on_null(error_code&)
    {
        stack.emplace_back(nullptr);
        return true;
    }
    inline bool on_comment_part(string_view, error_code&)
    {
        return true;
    }
    inline bool on_comment(string_view, error_code&)
    {
        return true;
    }
};

class Serializer : public boost::json::serializer
{
    union
    {
        JSON const* pj_;
        std::span<KeyJSONPair const> const* po_;
        std::span<JSON const> const* pa_;
    };
    JSON const* jj_ = nullptr;
    boost::json::value boost_value;
public:

    bool suspend(
        state st,
        std::span<JSON const>::iterator it,
        std::span<JSON const> const* pa)
    {
        st_.push(pa);
        st_.push(it);
        st_.push(st);
        return false;
    }

    bool suspend(
        state st,
        std::span<KeyJSONPair const>::iterator it,
        std::span<KeyJSONPair const> const* po)
    {
        st_.push(po);
        st_.push(it);
        st_.push(st);
        return false;
    }

    void reset(JSON const* j) noexcept
    {
        //pv_ = j;
        pj_ = j;
        fn0_ = (fn_t)&Serializer::write_value<true>;
        fn1_ = (fn_t)&Serializer::write_value<false>;

        jj_ = j;
        st_.clear();
        done_ = false;
    }

    template<bool StackEmpty>
    bool write_array(stream& ss0)
    {
        std::span<JSON const> const* pa;
        local_stream ss(ss0);
        std::span<JSON const>::iterator it, end;
        if(StackEmpty || st_.empty())
        {
            pa = pa_;
            it = pa->begin();
            end = pa->end();
        } else {
            state st;
            st_.pop(st);
            st_.pop(it);
            st_.pop(pa);
            end = pa->end();
            switch(st)
            {
            default:
            case state::arr1: goto do_arr1;
            case state::arr2: goto do_arr2;
            case state::arr3: goto do_arr3;
            case state::arr4: goto do_arr4;
                break;
            }
        }
do_arr1:
        if(BOOST_JSON_LIKELY(ss))
            ss.append('[');
        else
            return suspend(
                state::arr1, it, pa);
        if(it == end)
            goto do_arr4;
        for(;;)
        {
do_arr2:
            jj_ = &*it;
            if(! write_value<StackEmpty>(ss))
                return suspend(
                    state::arr2, it, pa);
            if(BOOST_JSON_UNLIKELY(
                ++it == end))
                break;
do_arr3:
            if(BOOST_JSON_LIKELY(ss))
                ss.append(',');
            else
                return suspend(
                    state::arr3, it, pa);
        }
do_arr4:
        if(BOOST_JSON_LIKELY(ss))
            ss.append(']');
        else
            return suspend(
                state::arr4, it, pa);
        return true;
    }

    template<bool StackEmpty>
    bool write_object(stream& ss0)
    {
        std::span<KeyJSONPair const> const* po;
        local_stream ss(ss0);
        std::span<KeyJSONPair const>::iterator it, end;
        if(StackEmpty || st_.empty())
        {
            po = po_;
            it = po->begin();
            end = po->end();
        } else {
            state st;
            st_.pop(st);
            st_.pop(it);
            st_.pop(po);
            end = po->end();
            switch(st)
            {
            default:
            case state::obj1: goto do_obj1;
            case state::obj2: goto do_obj2;
            case state::obj3: goto do_obj3;
            case state::obj4: goto do_obj4;
            case state::obj5: goto do_obj5;
            case state::obj6: goto do_obj6;
                break;
            }
        }
do_obj1:
        if(BOOST_JSON_LIKELY(ss))
            ss.append('{');
        else
            return suspend(
                state::obj1, it, po);
        if(BOOST_JSON_UNLIKELY(
            it == end))
            goto do_obj6;
        for(;;)
        {
            cs0_ = {
                it->first.data(),
                it->first.size() };
do_obj2:
            if(BOOST_JSON_UNLIKELY(
                ! write_string<StackEmpty>(ss)))
                return suspend(
                    state::obj2, it, po);
do_obj3:
            if(BOOST_JSON_LIKELY(ss))
                ss.append(':');
            else
                return suspend(
                    state::obj3, it, po);
do_obj4:
            jj_ = &it->second;
            if(BOOST_JSON_UNLIKELY(
                ! write_value<StackEmpty>(ss)))
                return suspend(
                    state::obj4, it, po);
            ++it;
            if(BOOST_JSON_UNLIKELY(it == end))
                break;
do_obj5:
            if(BOOST_JSON_UNLIKELY(ss))
                ss.append(',');
            else
                return suspend(
                    state::obj5, it, po);
        }
do_obj6:
        if(BOOST_JSON_LIKELY(ss))
        {
            ss.append('}');
            return true;
        }
        return suspend(
            state::obj6, it, po);
    }

    template<bool StackEmpty>
    bool write_value(stream& ss)
    {
        auto const& jj(*jj_);
        if (StackEmpty || st_.empty()) {
            switch(jj.index()) {
            default:
            case JSON::OBJECT:
                po_ = (std::span<KeyJSONPair const>const*)&jj.object();
                return write_object<true>(ss);
            case JSON::ARRAY:
                pa_ = (std::span<JSON const>const*)&jj.array();
                return write_array<true>(ss);
            case JSON::STRING:
            {
                auto const& js = std::get<std::string_view>(jj);
                cs0_ = { js.data(), js.size() };
                return write_string<true>(ss);
            }
            case JSON::INTEGER:
                boost_value = std::get<long>(jj);
                jv_ = &boost_value;
                return write_number<true>(ss);
            case JSON::NUMBER:
                boost_value = std::get<double>(jj);
                jv_ = &boost_value;
                return write_number<true>(ss);
            case JSON::BOOLEAN:
                if (std::get<bool>(jj)) {
                    if(BOOST_JSON_LIKELY(
                        ss.remain() >= 4))
                    {
                        ss.append("true", 4);
                        return true;
                    }
                    return write_true<true>(ss);
                } else {
                    if(BOOST_JSON_LIKELY(
                        ss.remain() >= 5))
                    {
                        ss.append("false", 5);
                        return true;
                    }
                    return write_false<true>(ss);
                }
            case (JSON::Index)NULL:
                if(BOOST_JSON_LIKELY(
                    ss.remain() >= 4))
                {
                    ss.append("null", 4);
                    return true;
                }
                return write_null<true>(ss);
            }
        } else {
            state st;
            st_.peek(st);
            switch(st)
            {
            default:
                return serializer::write_value<StackEmpty>(ss);

            case state::arr1: case state::arr2:
            case state::arr3: case state::arr4:
                return write_array<StackEmpty>(ss);

            case state::obj1: case state::obj2:
            case state::obj3: case state::obj4:
            case state::obj5: case state::obj6:
                return write_object<StackEmpty>(ss);
            }
        }
    }
};

thread_local boost::json::basic_parser<ParseHandler> parser({});

}

JSON::Doc JSON::decode(std::string_view text)
{
    std::error_code ec;
    parser.reset();
    try {
        /*size_t parsed_size = */parser.write_some(false, text.data(), text.size(), ec);
        if (ec) { throw std::system_error(ec); }
    } catch(...) {
        parser.handler().abort();
        throw;
    }
    return parser.handler().get();
}

std::string_view JSON::encode() const
{
    static thread_local Serializer serializer;
    static thread_local std::string storage;
    serializer.reset(this);
    storage.clear();
    while (!serializer.done()) {
        if (storage.capacity() == storage.size()) {
            storage.reserve(storage.size() * 2);
        }
        storage += serializer.read(&*storage.end(), storage.capacity() - storage.size());
    }
    return storage;
}

std::partial_ordering JSON::operator<=>(JSON const& json) const
{
    if (index() != json.index()) {
        return std::partial_ordering::unordered;
    }
    if (auto sv = std::get_if<std::string_view>(this)) {
        return *sv <=> std::get<std::string_view>(json);
    } else if (auto d = std::get_if<double>(this)) {
        return *d <=> std::get<double>(json);
    } else if (auto l = std::get_if<long>(this)) {
        return *l <=> std::get<long>(json);
    } else if (auto b = std::get_if<bool>(this)) {
        return *b <=> std::get<bool>(json);
    } else if (auto span = std::get_if<std::span<JSON>>(this)) {
        auto &lhs = *span;
        auto &rhs = std::get<std::span<JSON>>(json);
        std::partial_ordering cmp = std::partial_ordering::equivalent;
        for (size_t idx = 0; 0 == cmp && lhs.size() > idx; ++ idx) {
            cmp = lhs[idx] <=> rhs[idx];
        }
        return cmp;
    } else if (auto span = std::get_if<std::span<KeyJSONPair>>(this)) {
        static thread_local std::map<KeyJSONPair::first_type,KeyJSONPair::second_type> lhs, rhs;
        auto & rhs_ =std::get<std::span<KeyJSONPair>>(json);
        lhs.clear(); lhs.insert(span->begin(), span->end());
        rhs.clear(); rhs.insert(rhs_.begin(), rhs_.end());
        if (lhs.size() != rhs.size()) {
            return lhs.size() <=> rhs.size();
        }
        std::partial_ordering cmp = std::partial_ordering::equivalent;
        for (auto lit = lhs.begin(), rit = rhs.begin(); lit != lhs.end() && rit != rhs.end(); ++ lit, ++ rit) {
            auto & [lk, lv] = *lit;
            auto & [rk, rv] = *rit;
            cmp = lk <=> rk; if (cmp != 0) break;
            cmp = lv <=> rv; if (cmp != 0) break;
        }
        return cmp;
    } else {
        return std::partial_ordering::equivalent;
    }
}

JSON const& JSON::operator[](std::string_view key) const
{
    JSON * result = nullptr;
    for (auto & [k, json] : object()) {
        if (k == key) {
            if (result != nullptr) {
                throw std::out_of_range("multiple entries");
            }
            result = &json;
        }
    }
    if (result == nullptr) {
        throw std::out_of_range("not found");
    }
    return *result;
}

JSON const& JSON::operator[](size_t idx) const
{
    return array()[idx];
}

JSON::Doc::Doc(JSON*root)
: root_(root)
{ }

JSON::Doc::Doc(Doc&&doc)
: root_(doc.root_)
{
    doc.root_ = nullptr;
}

JSON::Doc::~Doc()
{
    if (root_) {
        ParseHandler::del(root_);
    }
}

}
