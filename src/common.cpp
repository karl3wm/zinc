#include <zinc/common.hpp>

#include <algorithm>
#include <vector>

namespace zinc {

std::string replaced(
    std::string_view haystack,
    std::span<StringViewPair const> replacements
)
{
    static thread_local std::vector<std::pair<size_t, size_t>> off_repl_pairs;
    static thread_local std::vector<std::pair<size_t, size_t>> off_repl_list;
    size_t new_size = haystack.size();
    off_repl_pairs.clear();
    off_repl_list.clear();

    // iterate through the string, using off_repl_pairs to track substrs
    off_repl_pairs.resize(replacements.size());
    for (size_t i = 0; i < replacements.size(); ++ i) {
        off_repl_pairs[i].first = haystack.find(replacements[i].first, 0);
        off_repl_pairs[i].second = i;
    }
    std::sort(off_repl_pairs.begin(), off_repl_pairs.end());
    while (off_repl_pairs.front().first != std::string::npos) {
        auto off_repl_pair = off_repl_pairs.front();
        auto & replacement = replacements[off_repl_pair.second];
        off_repl_list.emplace_back(off_repl_pair);
        new_size += replacement.second.size();
        new_size -= replacement.first.size();
        off_repl_pair.first += replacement.first.size();
        off_repl_pair.first = haystack.find(replacement.first, off_repl_pair.first);
        auto sort = std::lower_bound(off_repl_pairs.begin(), off_repl_pairs.end(), off_repl_pair);
        if (sort > off_repl_pairs.begin()) {
	        std::copy(off_repl_pairs.begin()+1, sort, off_repl_pairs.begin());
            -- sort;
	        *sort = off_repl_pair;
        }
    }

    // make the string in one pass using off_repl_list
    std::string replaced(new_size, '\0');
    size_t offset_old = 0;
    ssize_t new_minus_old = 0;
    for (auto off_repl_pair : off_repl_list) {
        auto & replacement = replacements[off_repl_pair.second];
        // write skipped region
        size_t skipped_size = off_repl_pair.first - offset_old;
        replaced.replace(
            offset_old + (size_t)new_minus_old, skipped_size,
            &haystack[offset_old], skipped_size
        );
        // write replacement
        replaced.replace(
            off_repl_pair.first + (size_t)new_minus_old,
            replacement.second.size(),
            replacement.second
        );
        new_minus_old += (ssize_t)replacement.second.size();
        new_minus_old -= (ssize_t)replacement.first.size();
        offset_old = off_repl_pair.first + replacement.first.size();
    }
    // write skipped region
    size_t skipped_size = haystack.size() - offset_old;
    replaced.replace(
        offset_old + (size_t)new_minus_old, skipped_size,
        &haystack[offset_old], skipped_size
    );

    return replaced;
}

} // namespace zinc
