#include <zinc/diff.hpp>

#include <string_view>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <deque>
#include <functional>
#include <chrono>
#include <filesystem>

namespace zinc {

//struct LineLocation {
//    std::streamoff offset;
//    size_t length;
//    size_t line_number;
//};

//struct HunkState {
//    bool is_active = false;
//    size_t old_start = 0;
//    size_t new_start = 0;
//    std::deque<std::string> pending_output;
//
//    generator<std::string_view> flush()
//    {
//        if (is_active && !pending_output.empty()) {
//            {
//                std::stringstream header;
//                header << "@@ -" << old_start << " +" << new_start << " @@";
//                co_yield header.str();
//            }
//
//            for (const auto& line : pending_output) {
//                co_yield line;
//            }
//
//            pending_output.clear();
//            is_active = false;
//        }
//    }
//};

//static std::string readLine(std::ifstream& file, const LineLocation& loc) {
//    std::string line;
//    line.resize(loc.length);
//    file.seekg(loc.offset);
//    file.read(line.data(), loc.length);
//    if (!line.empty() && line.back() == '\n') {
//        line.pop_back();
//    }
//    return line;
//}

static std::string formatTimestamp(const std::filesystem::file_time_type& ftime) {
    std::chrono::system_clock::time_point sctp(std::chrono::duration_cast<std::chrono::system_clock::duration>(ftime.time_since_epoch()));
    auto tt = std::chrono::system_clock::to_time_t(sctp);
    std::string timestamp = std::ctime(&tt);
    if (!timestamp.empty() && timestamp.back() == '\n') {
        timestamp.pop_back();
    }
    return timestamp;
}

struct UnifiedDiffGenerator_
{
    UnifiedDiffGenerator_(
        size_t lines_ctx
      , std::span<size_t> offsets
      , std::ifstream& file
    ) : lines_ctx(lines_ctx)
      , offsets(offsets)
      , file(file)
      , line_change_old(-(ssize_t)lines_ctx*2)
      , line_next_old(0)
      , lines_new_minus_old(0)
    {
    }

    zinc::generator<std::string_view>
    header(
        std::string_view fn_old, std::filesystem::file_time_type time_old,
        std::string_view fn_new, std::filesystem::file_time_type time_new
    ) {
        (ss={}) << "--- " << fn_old << "\t" << formatTimestamp(time_old);
        co_yield ss.view();

        (ss={}) << "+++ " << fn_new << "\t" << formatTimestamp(time_new);
        co_yield ss.view();
    }

    zinc::generator<std::string_view>
    start_change_() {
        if (!active_()) {
            // new hunk
            // rewind by lines_ctx
            size_t line_old = (size_t)std::max((ssize_t)line_next_old - (ssize_t)lines_ctx, (ssize_t)0);
            // hunk header
            (ss={}) << "@@ -" << (line_old+1)
                    << " +" << ((ssize_t)line_old+lines_new_minus_old+1)
                    << " @@";
            co_yield ss.view();
            // preceding context
            for (; line_old < line_next_old; ++ line_old) {
                (ss={}) << ' '; insert_line_(line_old);
                co_yield ss.view();
            }
        }
        line_change_old = (ssize_t)line_next_old;
    }

    zinc::generator<std::string_view>
    deleted()
    {
        co_yield zinc::ranges::elements_of(start_change_());

        (ss={}) << '-'; insert_line_(line_next_old);
        co_yield ss.view();

        ++ line_next_old;
        -- lines_new_minus_old;
    }

    zinc::generator<std::string_view>
    deleted_until(size_t line_old)
    {
        // all lines up to but not including line_old are deleted
        assert(line_old >= line_next_old);
        while (line_old != line_next_old) {
            co_yield zinc::ranges::elements_of(deleted());
        }
    }

    zinc::generator<std::string_view>
    added(std::string_view str_new)
    {
        co_yield zinc::ranges::elements_of(start_change_());

        (ss={}) << '+' << str_new;
        co_yield ss.view();

        ++ lines_new_minus_old;
    }

    zinc::generator<std::string_view>
    same()
    {
        if (active_()) {
            // trailing context
            (ss={}) << ' '; insert_line_(line_next_old);
            co_yield ss.view();
        }
        ++ line_next_old;
    }

    zinc::generator<std::string_view>
    same_at(size_t line_old)
    {
        // output skipped lines as deletions
        co_yield zinc::ranges::elements_of(deleted_until(line_old));
        co_yield zinc::ranges::elements_of(same());
    }

    bool active_()
    {
        return line_change_old + (ssize_t)lines_ctx >= (ssize_t)line_next_old;
    }

    void insert_line_(size_t num)
    {
        std::string str(std::move(ss.str()));
        size_t offset_o = str.size();
        size_t offset_i = offsets[num];
        size_t size_i = offsets[num+1] - offset_i;
        str.resize(offset_o + size_i);
        file.seekg((ssize_t)offset_i);
        file.read(str.data() + offset_o, (ssize_t)size_i);
        if (str.size() > offset_o && str.back() == '\n') {
            str.resize(str.size() - 1);
        }
        ss.str(std::move(str));
    }

    size_t lines_ctx;
    std::span<size_t> offsets;
    std::ifstream& file;
    ssize_t line_change_old;
    size_t line_next_old;
    ssize_t lines_new_minus_old;
    std::stringstream ss;
};

generator<std::string_view> UnifiedDiffGenerator::generateDiff(
    const std::filesystem::path& filepath,
    std::ifstream& file,
    generator<std::string_view> newContent,
    size_t context_lines
) {
    // Build multimap of line hashes to line locations
    //std::unordered_multimap<size_t, LineLocation> line_map;
    //std::vector<LineLocation> line_locations;
    std::unordered_multimap<size_t, size_t> line_by_hash;
    std::vector<size_t> line_offsets;
    //std::string line;
    //size_t line_num = 1;  // 1-based line numbers
    std::hash<std::string_view> hasher;

    // Record file positions and build hash map
    {
        std::string line;
        file.seekg(0);
        std::streamoff current_pos = file.tellg();
        while (std::getline(file, line)) {
            //size_t length = line.length() + 1;  // Include newline
            //LineLocation loc{current_pos, length, line_num};
            //line_locations.push_back(loc);
            line_by_hash.emplace(hasher(line), line_offsets.size());
            line_offsets.push_back((size_t)current_pos);
            current_pos = file.tellg();
    
            //size_t hash = hasher(std::string_view(line));
            //line_map.emplace(hash, loc);
    
            //current_pos += length;
            //line_num++;
        }
        line_offsets.push_back((size_t)current_pos);
        file.clear(); // clear failbit from reading at eof so further reads succeed
    }
    
    UnifiedDiffGenerator_ impl(context_lines, line_offsets, file);

    //std::stringstream ss;

    // Generate header
    {
        auto fileTime = std::filesystem::last_write_time(filepath);
        std::string filepathstr = filepath.string();

        co_yield zinc::ranges::elements_of(impl.header(
            filepathstr, fileTime,
            filepathstr, std::filesystem::file_time_type::clock::now()
        ));

        //(ss={}) << "--- " << filepathstr << "\t" << formatTimestamp(fileTime);
        //co_yield ss.str();

        //(ss={}) << "+++ " << filepathstr << "\t" << formatTimestamp(std::filesystem::file_time_type::clock::now());
        //co_yield ss.str();

    }

    // Process incoming lines
    //size_t expected_line = 1;
    //size_t new_line_num = 1;
    //ssize_t hunk_old_line = -context_lines*2;
    //ssize_t hunk_new_line = -context_lines*2;
    //HunkState current_hunk;
    //std::deque<std::string> context_buffer;

    /*
    auto startNewHunk = [&](size_t old_line, size_t new_line) {
        if (old_line > hunk_old_line + context_lines * 2) {
            // new hunk
            // trailing context
            if (hunk_old_line >= 1) {
                size_t tail = hunk_old_line + context_lines;
                for(; hunk_old_line < tail; ++ hunk_old_line) {
                    (ss={}) << ' ' << readLine(file, line_locations[hunk_old_line-1]);
                    co_yield ss.str();
                }
            }
            // hunk header
            hunk_old_line = max(old_line - context_lines, 1);
            hunk_new_line = hunk_old_line + new_line - old_line;
            (ss={}) << "@@ -" << hunk_old_line << " +" << hunk_new_line << "@@";
            co_yield ss.str();
        }
        // preceding context
        for (; hunk_old_line < old_line; ++ hunk_old_line) {
            (ss={}) << ' ' << readLine(file, line_locations[hunk_old_line-1]);
        }
        hunk_new_line = new_line;
    };
    */

    for (std::string_view new_line : newContent) {
        //bool found = false;
        //size_t line_num = SIZE_MAX;
        //LineLocation matched_loc;

        // Find the first matching line that hasn't been processed
        auto [match_start, match_end] = line_by_hash.equal_range(hasher(new_line));
        
        //if (match_start != line_map.end()) {
        //}
        //for (; match_start != match_end; ++
        //for (auto & num : line_map.equal_range(hasher(new_line))) {
        //    if (num >= expected_line &&
        //        loc_line_number < line_num
        //    ) {
        //        line_num = loc.line_number;
        //        found = true;
        //    }
        //}

        //if (!found || line_num > hunk_old_line) {
        //    startNewHunk(line_num, hunk_new_line);
        //}
        if (match_start != line_by_hash.end()) {
            // the new line matches a line from the file
            auto match = match_start, match_it = match;
            for (++ match_it; match_it != match_end; ++ match_it) {
                // find the lowest line number
                if (match_it->second < match->second) {
                    match = match_it;
                }
            }

            co_yield zinc::ranges::elements_of(impl.same_at(match->second));
            line_by_hash.erase(match);
            //// Output any skipped lines as deletions
            //if (line_num > hunk_old_line) {
            //    for (; hunk_old_line < line_num; ++ hunk_old_line) {
            //        (ss={}) << '-' << readLine(file, line_locations[hunk_old_line - 1]);
            //        co_yield ss.str();
            //    }
            //}
            //++ hunk_old_line;
            //++ hunk_new_line;
        } else {
            // Line not found - it's an addition
            co_yield zinc::ranges::elements_of(impl.added(new_line));
            //startNewHunk(hunk_old_line, hunk_new_line);
            //(ss={}) << '+' << new_line;
            //co_yield ss.str();
            //++ hunk_new_line;
        }

        //new_line_num++;
    }

    // Output remaining lines from original as deletions
    co_yield zinc::ranges::elements_of(impl.deleted_until(line_offsets.size()-1));
    //if (hunk_old_line <= line_locations.size()) {
    //    startNewHunk(hunk_old_line, hunk_new_line);

    //    while (hunk_old_line <= line_locations.size()) {
    //        (ss={}) << '-' << readLine(file, line_locations[hunk_old_line - 1]);
    //        co_yield ss.str();
    //        ++ hunk_old_line;
    //    }
    //}

    //// Flush final hunk if active
    //if (hunk_old_line >= 1) {
    //    size_t tail = min(hunk_old_line + context_lines, line_locations.size());
    //    for(; hunk_old_line < tail; ++ hunk_old_line) {
    //        (ss={}) << ' ' << readLine(file, line_locations[hunk_old_line-1]);
    //    }
    //}
}

} // namespace zinc
