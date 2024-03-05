#include "Utility.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace util {

// https://stackoverflow.com/questions/11413860/best-string-hashing-function-for-short-filenames
uint32_t fnv_hash(const void* key, int len) {
  const unsigned char* const p = (unsigned char*)key;
  unsigned int h = 2166136261;

  for (int i = 0; i < len; i++) {
    h = (h * 16777619) ^ p[i];
  }

  return h;
}

void writeFile(const std::string& filePath,
               const std::vector<char>& fileContents, bool isBinary) {
  if (isBinary) {
    std::ofstream out(filePath, std::ios::app | std::ios::binary);
    out.write(fileContents.data(), fileContents.size());
    out.close();
  } else {
    std::ofstream out(filePath);
    out << std::string(fileContents.data());
    out.close();
  }
}

std::vector<char> readFile(const std::string& filePath, bool isBinary) {
  std::ios_base::openmode mode = std::ios::ate;
  if (isBinary) {
    mode |= std::ios::binary;
  }
  std::ifstream file(filePath, mode);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = (size_t)file.tellg();
  if (!isBinary) {
    fileSize += 1;  // add extra for null char at end
  }
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
  file.close();
  if (!isBinary) {
    buffer[buffer.size() - 1] = '\0';
  }
  return buffer;
}

int endsWith(const char* s, const char* part) {
  return (strstr(s, part) - s) == (strlen(s) - strlen(part));
}

std::unordered_set<std::string> filterExtensions(
    std::vector<std::string> availableExtensions,
    std::vector<std::string> requestedExtensions) {
  std::sort(availableExtensions.begin(), availableExtensions.end());
  std::sort(requestedExtensions.begin(), requestedExtensions.end());
  std::vector<std::string> result;
  std::set_intersection(availableExtensions.begin(), availableExtensions.end(),
                        requestedExtensions.begin(), requestedExtensions.end(),
                        std::back_inserter(result));
  return std::unordered_set<std::string>(result.begin(), result.end());
}

}  // namespace util
