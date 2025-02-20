#include <zinc/common.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace {
struct SubstringFinder
{

    void reset(std::string_view haystack)
    {
        this->haystack = haystack;
        needles.clear();
        off_needle_pairs.clear();
    }

    void add_needle(std::string_view needle)
    {
        size_t i = needles.size();
        needles.emplace_back(needle);
        off_needle_pairs.emplace_back(haystack.find(needle), i);
    }

    void begin()
    {
        std::sort(off_needle_pairs.begin(), off_needle_pairs.end());
    }

    bool has_more()
    {
        return off_needle_pairs.front().first != std::string_view::npos;
    }

    std::pair<size_t, size_t> find_next()
    {
        auto updated = off_needle_pairs.front();
        auto result = updated;
        auto & [off, idx] = updated;
        auto & needle = needles[idx];
        off += needle.size();
        off = haystack.find(needle, off);
        auto sort = std::lower_bound(off_needle_pairs.begin(), off_needle_pairs.end(), updated);
        if (sort > off_needle_pairs.begin()) {
	        std::copy(off_needle_pairs.begin()+1, sort, off_needle_pairs.begin());
            -- sort;
        }
        *sort = updated;
        return result;
    }

    std::string_view haystack;
    std::vector<std::string_view> needles;
    std::vector<std::pair<size_t, size_t>> off_needle_pairs;
};

SubstringFinder & substring_finder() {
    static thread_local SubstringFinder substring_finder;
    return substring_finder;
}
}

namespace zinc {

std::span<std::pair<size_t, size_t>> find_all_of(
    std::string_view haystack,
    std::span<std::string_view> needles
)
{
    static thread_local std::vector<std::pair<size_t, size_t>> matches;

    substring_finder().reset(haystack);
    for (auto & needle : needles)
        substring_finder().add_needle(needle);
    substring_finder().begin();

    matches.clear();
    while (substring_finder().has_more()) {
        matches.emplace_back(substring_finder().find_next());
    }

    return matches;
}

std::pair<size_t, size_t> find_first_of(
    std::string_view haystack,
    std::span<std::string_view> needles
)
{
    substring_finder().reset(haystack);
    for (auto needle : needles)
        substring_finder().add_needle(needle);
    substring_finder().begin();

    return substring_finder().find_next();
}

std::string_view replaced(
    std::string_view haystack,
    std::span<StringViewPair const> replacements
)
{
    static thread_local std::string replaced;

    substring_finder().reset(haystack);
    for (auto [needle, repl] : replacements) {
        substring_finder().add_needle(needle);
    }
    substring_finder().begin();

    replaced.clear();
    size_t off_old = 0;
    while (substring_finder().has_more()) {
        auto [ off, idx ] = substring_finder().find_next();
        auto & [ _, repl ] = replacements[idx];
        replaced.append(&haystack[off_old], &haystack[off]);
        replaced.append(repl);
        off_old = off + repl.size();
    }
    replaced.append(&haystack[off_old], &*haystack.end());

    return replaced;
}

std::string_view trim(std::string_view str)
{
    char const* start = &*std::find_if_not(str.begin(), str.end(), [](char c){return std::isspace(c);});
    char const* end = &*std::find_if_not(str.rbegin(), str.rend(), [](char c){return std::isspace(c);}) + 1;
    return start < end ? std::string_view(start, (size_t)(end - start)) : std::string_view();
}

generator<std::string_view> shell(std::string_view cmdline_)
{
    static thread_local std::string cmdline;
    static thread_local std::string buffer;
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        throw std::runtime_error("pipe()");
    }
    cmdline = cmdline_;
    pid_t pid = fork();
    if (pid == -1) {
        throw std::runtime_error("fork()");
    }
    if (pid == 0) {
        // child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        char const* shell = std::getenv("SHELL");
        if (shell) {
            execl(shell, shell, "-c", cmdline.c_str(), nullptr);
        } else {
            execl("/bin/sh", "sh", "-c", cmdline.c_str(), nullptr);
        }
        write(STDERR_FILENO, "failed: execl()\n", strlen("failed: execl()\n"));
        exit(-1);
    } else {
        // parent
        close(pipefd[1]);
        buffer.reserve(1024);
        buffer.resize(buffer.capacity());
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
            co_yield std::string_view(buffer).substr(0, (size_t)bytes_read);
        }
        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            //int exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            //std::string info = WTERMSIG(status);
        }
    }
}

} // namespace zinc
