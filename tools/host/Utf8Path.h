#pragma once

// Host tools carry every path as a UTF-8 std::string and convert only where
// they touch the OS. On Windows the plain narrow APIs disagree with each
// other: libstdc++'s fs::path(std::string) assumes UTF-8 (and throws on
// anything else) while std::fstream(std::string), getenv() and argv use the
// ANSI code page. So never build an fs::path from a std::string or open a
// stream on a std::string path directly: go through these helpers.

#include <filesystem>
#include <string>
#include <vector>

namespace ledbasic {

// UTF-8 -> path. Returns an empty path (which every filesystem call rejects)
// for input that isn't valid UTF-8, instead of throwing.
std::filesystem::path pathFromUtf8(const std::string& utf8);

// path -> UTF-8 in native ('\' on Windows) or generic ('/') form.
std::string utf8FromPath(const std::filesystem::path& p);
std::string genericUtf8FromPath(const std::filesystem::path& p);

// Environment variable as UTF-8, or "" when unset.
std::string envUtf8(const char* name);

// Command-line arguments as UTF-8 (argv itself is ANSI on Windows).
std::vector<std::string> utf8Args(int argc, char** argv);

} // namespace ledbasic
