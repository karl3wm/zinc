#pragma once

#include <string>
#include <string_view>
#include <chrono>

namespace zinc {

class Log {
public:
    static void init(std::string_view filename);
    static void log(std::string_view role, std::string_view content);

private:
    static std::ofstream logFile_;
    static std::chrono::system_clock::time_point startTime_;
};

std::string escaped(std::string_view str);

} // namespace zinc
