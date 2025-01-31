#include <zinc/log.hpp>
#include <boost/json/src.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace zinc {

class Log {
public:
    static void log(std::span<StringViewPair> fields) {
        static std::ofstream logf;
        static std::stringstream logfn_ss;

        if (!logf.is_open()) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm now_tm = *std::localtime(&now_time_t);
            logfn_ss << std::put_time(&now_tm, "%FT%TZ.log");
            logf.open(logfn_ss.str());
            logf << std::fixed << std::setprecision(3);
        }

        boost::json::object obj;
        for (auto&& [key, value] : fields) {
            obj[key] = value;
        }

        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        obj["ts"] = now_ms / 1000.0;

        logf << boost::json::serialize(obj) << std::endl;
    }
};

} // namespace zinc
