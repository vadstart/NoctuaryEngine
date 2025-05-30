#pragma once

#include <filesystem>
#include <functional>

namespace nt {

inline std::filesystem::path getProjectRoot() {
  static std::filesystem::path cachedRoot = [] {
    auto path = std::filesystem::current_path();
    while (!std::filesystem::exists(path / "xmake.lua")) {
      path = path.parent_path();
      if (path.empty()) {
        throw std::runtime_error("Failed to locate project root (xmake.lua not found)");
      }
    }
    return path;
  }();
  return cachedRoot;
}

inline std::string getAssetPath(const std::string& relPath) {
  static std::filesystem::path root = getProjectRoot();
  return (root / relPath).string();
}

// From https://stackoverflow.com/a/57595105
template <typename T, typename... Rest>
void hashCombine(std::size_t& seed, const T& v, const Rest&... rest) {
  seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  (hashCombine(seed, rest), ...);
}

}
