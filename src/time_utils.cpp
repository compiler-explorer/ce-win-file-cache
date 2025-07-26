#include "../include/ce-win-file-cache/time_utils.hpp"

namespace CeWinFileCache
{

std::wstring TimeUtils::formatDuration(std::chrono::system_clock::duration duration)
{
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    if (seconds < 60)
        return std::to_wstring(seconds) + L" seconds";
    else if (seconds < 3600)
        return std::to_wstring(seconds / 60) + L" minutes";
    else if (seconds < 86400)
        return std::to_wstring(seconds / 3600) + L" hours";
    else
        return std::to_wstring(seconds / 86400) + L" days";
}

std::wstring TimeUtils::getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();

    return formatTimestamp(now, L"%Y%m%d_%H%M%S");
}

std::wstring TimeUtils::formatTimestamp(std::chrono::system_clock::time_point tp, const wchar_t *format)
{
    auto time_t = std::chrono::system_clock::to_time_t(tp);

    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    std::wostringstream oss;
    oss << std::put_time(&tm, format);
    return oss.str();
}

} // namespace CeWinFileCache