#include <zinc/log.hpp>
#include <zinc/configuration.hpp>
#include <boost/json.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace zinc {

static std::ofstream & logf()
{
    static struct LogStream : public std::ofstream
    {
    public:
        LogStream()
        {
            std::stringstream logfn_ss;
            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm now_tm = *std::localtime(&now_time_t);
            logfn_ss << std::put_time(&now_tm, "%FT%TZ.log");
            open(Configuration::path_local(zinc::span<std::string_view>({
                "logs",
                logfn_ss.str()
            })).begin());
        }
    } logf;
    return logf;
}

void Log::log(std::span<StringViewPair const> fields)
{
    boost::json::object obj;

    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    for (auto&& [key, value] : fields) {
        obj[key] = value;
    }

    obj["ts"] = now_ms / 1000.0;

    logf() << obj << std::endl;
}

static struct EnsureLogfileCreated
{
    EnsureLogfileCreated()
    { logf(); }
} ensure_logfile_created;

} // namespace zinc
