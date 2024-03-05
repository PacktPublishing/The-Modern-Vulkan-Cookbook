#pragma once

#include <cassert>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#define ASSERT(expr, message) \
  {                           \
    void(message);            \
    assert(expr);             \
  }

#define MOVABLE_ONLY(CLASS_NAME)                     \
  CLASS_NAME(const CLASS_NAME&) = delete;            \
  CLASS_NAME& operator=(const CLASS_NAME&) = delete; \
  CLASS_NAME(CLASS_NAME&&) noexcept = default;       \
  CLASS_NAME& operator=(CLASS_NAME&&) noexcept = default;

namespace util {

// https://stackoverflow.com/questions/11413860/best-string-hashing-function-for-short-filenames
uint32_t fnv_hash(const void* key, int len);

std::vector<char> readFile(const std::string& filePath, bool isBinary = false);

void writeFile(const std::string& filePath,
               const std::vector<char>& fileContents, bool isBinary = false);

int endsWith(const char* s, const char* part);

std::unordered_set<std::string> filterExtensions(
    std::vector<std::string> availableExtensions,
    std::vector<std::string> requestedExtensions);

template <typename T, typename... Rest>
void hash_combine(std::size_t& seed, const T& v, const Rest&... rest) {
  seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  (hash_combine(seed, rest), ...);
}

}  // namespace util
