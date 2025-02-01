#include <zinc/log.hpp>
#include <zinc/configuration.hpp>
#include <boost/json.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace zinc {

static std::tm & launch_time()
{
    static struct LaunchTime : public std::tm
    {
        LaunchTime()
        {
            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            *(std::tm*)this = *std::localtime(&now_time_t);
        }
    } launch_tm;
    return launch_tm;
}

static std::ofstream & logf()
{
    static struct LogStream : public std::ofstream
    {
        LogStream()
        {
            std::stringstream logfn_ss;
            logfn_ss << std::put_time(&launch_time(), "%FT%TZ.log");
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

static struct EnsureLaunchTimeCreated
{
    EnsureLaunchTimeCreated()
    { launch_time(); }
} ensure_launchtime_created;

} // namespace zinc
