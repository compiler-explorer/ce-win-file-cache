#pragma once

#include <chrono>
#include <string>

namespace CeWinFileCache
{

const wchar_t *const TIME_FORMAT_DEFAULT = L"%Y-%m-%d %H:%M:%S";

class TimeUtils
{
   public:
      static std::wstring formatDuration(std::chrono::system_clock::duration duration);
      static std::wstring getCurrentTimestamp();
      static std::wstring formatTimestamp(std::chrono::system_clock::time_point tp, const wchar_t *format = TIME_FORMAT_DEFAULT);
};

}

