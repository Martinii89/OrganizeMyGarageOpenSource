// ReSharper disable CppNonExplicitConvertingConstructor
#pragma once
#include <chrono>
#include <format>
#include <fstream>
#include <memory>
#include <source_location>
#include <string>

#include "CVarManagerSingleton.h"

constexpr bool DEBUG_LOG = false;
constexpr bool PERSISTENT_LOGGING = false;

struct FormatString {
  std::string_view str;
  std::source_location loc{};

  FormatString(const char* str, const std::source_location& loc =
                                    std::source_location::current())
      : str(str), loc(loc) {}
  FormatString(const std::string&& str, const std::source_location& loc =
                                            std::source_location::current())
      : str(str), loc(loc) {}

  [[nodiscard]] std::string GetLocation() const {
    return std::format("[{} ({}:{})]", loc.function_name(), loc.file_name(),
                       loc.line());
  }
};

struct FormatWstring {
  std::wstring_view str;
  std::source_location loc{};

  FormatWstring(const wchar_t* str, const std::source_location& loc =
                                        std::source_location::current())
      : str(str), loc(loc) {}
  FormatWstring(const std::wstring&& str, const std::source_location& loc =
                                              std::source_location::current())
      : str(str), loc(loc) {}

  [[nodiscard]] std::wstring GetLocation() const {
    auto basic_string = std::format("[{} ({}:{})]", loc.function_name(),
                                    loc.file_name(), loc.line());
    return std::wstring(basic_string.begin(), basic_string.end());
  }
};

inline void PERSIST(const std::string& str) {
  if constexpr (PERSISTENT_LOGGING) {
    std::ofstream of;
    of.open(std::filesystem::temp_directory_path() / "bakkesmod_omg.log",
            std::ios_base::app);

    using namespace std::chrono;
    auto local_time = zoned_time{current_zone(), system_clock::now()};

    of << std::format("[{}] ", local_time) << str << '\n';
  }
}

inline void PERSIST(const std::wstring& str) {
  if constexpr (PERSISTENT_LOGGING) {
    std::wofstream of;
    of.open(std::filesystem::temp_directory_path() / "bakkesmod_omg.log",
            std::ios_base::app);

    using namespace std::chrono;
    auto local_time = zoned_time{current_zone(), system_clock::now()};

    of << std::format(L"[{}] ", local_time) << str << '\n';
  }
}

template <typename... Args>
void LOG(std::string_view format_str, Args&&... args) {
  auto str = std::vformat(format_str,
                          std::make_format_args(std::forward<Args>(args)...));
  PERSIST(str);
  auto cvar = CVarManagerSingleton::getInstance().getCvar();
  if (cvar) cvar->log(std::move(str));
}

template <typename... Args>
void LOG(std::wstring_view format_str, Args&&... args) {
  auto str = std::vformat(format_str,
                          std::make_wformat_args(std::forward<Args>(args)...));
  PERSIST(str);
  auto cvar = CVarManagerSingleton::getInstance().getCvar();
  if (cvar) cvar->log(std::move(str));
}

template <typename... Args>
void DEBUGLOG(const FormatString& format_str, Args&&... args) {
  if constexpr (DEBUG_LOG) {
    auto str = std::format(
        "{} {}",
        std::vformat(format_str.str,
                     std::make_format_args(std::forward<Args>(args)...)),
        format_str.GetLocation());
    PERSIST(str);
    auto cvar = CVarManagerSingleton::getInstance().getCvar();
    if (cvar) cvar->log(std::move(str));
  }
}

template <typename... Args>
void DEBUGLOG(const FormatWstring& format_str, Args&&... args) {
  if constexpr (DEBUG_LOG) {
    auto str = std::format(
        L"{} {}",
        std::vformat(format_str.str,
                     std::make_wformat_args(std::forward<Args>(args)...)),
        format_str.GetLocation());
    PERSIST(str);
    auto cvar = CVarManagerSingleton::getInstance().getCvar();
    if (cvar) cvar->log(std::move(str));
  }
}
