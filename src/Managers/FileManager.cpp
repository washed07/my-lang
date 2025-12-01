#include "ml/Managers/FileManager.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ml {

FileManager::FileManager(StringInterner &interner) : interner(interner) {}

FileManager::~FileManager() { clearCache(); }

FileManager::FileManager(FileManager &&other) noexcept
    : fileCache(std::move(other.fileCache)), interner(other.interner),
      maxCacheSize(other.maxCacheSize),
      memoryMappingEnabled(other.memoryMappingEnabled),
      memoryMappingThreshold(other.memoryMappingThreshold), stats(other.stats) {
}

FileManager &FileManager::operator=(FileManager &&other) noexcept {
  if (this != &other) {
    clearCache();
    fileCache = std::move(other.fileCache);
    maxCacheSize = other.maxCacheSize;
    memoryMappingEnabled = other.memoryMappingEnabled;
    memoryMappingThreshold = other.memoryMappingThreshold;
    stats = other.stats;
  }
  return *this;
}

std::shared_ptr<FileEntry> FileManager::getFile(const std::string &filename) {
  auto [entry, error] = getFileWithError(filename);
  return entry; // Returns nullptr if there was an error
}

std::pair<std::shared_ptr<FileEntry>, std::error_code>
FileManager::getFileWithError(const std::string &filename) {
  std::string normalizedName = normalizeFilename(filename);
  InternedString internedName = interner.intern(normalizedName);

  {
    std::lock_guard<std::mutex> lock(cacheMutex);

    // Check cache first
    auto it = fileCache.find(internedName);
    if (it != fileCache.end()) {
      ++stats.cacheHitCount;
      return {it->second, {}};
    }

    ++stats.cacheMissCount;
  }

  // Load file from disk
  auto [entry, error] = loadFile(normalizedName);
  if (error) {
    return {nullptr, error};
  }

  {
    std::lock_guard<std::mutex> lock(cacheMutex);

    // Double-check that another thread didn't load it while we were loading
    auto it = fileCache.find(internedName);
    if (it != fileCache.end()) {
      ++stats.cacheHitCount;
      return {it->second, {}};
    }

    // Add to cache
    fileCache[internedName] = entry;
    ++stats.fileCacheCount;

    // Evict old files if needed
    evictIfNeeded();
  }

  return {entry, {}};
}

bool FileManager::fileExists(const std::string &filename) const {
  std::string normalizedName = normalizeFilename(filename);
  InternedString internedName = interner.intern(normalizedName);

  {
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (fileCache.count(internedName)) {
      return true;
    }
  }

  std::error_code ec;
  return std::filesystem::exists(normalizedName, ec) && !ec;
}

std::pair<size_t, std::error_code>
FileManager::getFileSize(const std::string &filename) const {
  std::string normalizedName = normalizeFilename(filename);
  InternedString internedName = interner.intern(normalizedName);

  {
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto it = fileCache.find(internedName);
    if (it != fileCache.end()) {
      return {it->second->getSize(), {}};
    }
  }

  std::error_code ec;
  auto size = std::filesystem::file_size(normalizedName, ec);
  if (ec) {
    return {0, ec};
  }

  return {static_cast<size_t>(size), {}};
}

std::pair<time_t, std::error_code>
FileManager::getFileModTime(const std::string &filename) const {
  std::string normalizedName = normalizeFilename(filename);
  InternedString internedName = interner.intern(normalizedName);

  {
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto it = fileCache.find(internedName);
    if (it != fileCache.end()) {
      return {it->second->getModificationTime(), {}};
    }
  }

  std::error_code ec;
  auto timePoint = std::filesystem::last_write_time(normalizedName, ec);
  if (ec) {
    return {0, ec};
  }

  // Convert to time_t
  auto duration = timePoint.time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  return {seconds.count(), {}};
}

void FileManager::clearCache() {
  std::lock_guard<std::mutex> lock(cacheMutex);
  fileCache.clear();
  stats.fileCacheCount = 0;
}

void FileManager::removeFromCache(const std::string &filename) {
  std::string normalizedName = normalizeFilename(filename);
  InternedString internedName = interner.intern(normalizedName);
  std::lock_guard<std::mutex> lock(cacheMutex);

  auto it = fileCache.find(internedName);
  if (it != fileCache.end()) {
    fileCache.erase(it);
    --stats.fileCacheCount;
  }
}

size_t FileManager::getCurrentCacheSize() const {
  std::lock_guard<std::mutex> lock(cacheMutex);

  size_t totalSize = 0;
  for (const auto &[filename, entry] : fileCache) {
    totalSize += entry->getSize();
  }

  return totalSize;
}

FileManagerStats FileManager::getStats() const {
  std::lock_guard<std::mutex> lock(cacheMutex);
  return stats;
}

std::pair<std::shared_ptr<FileEntry>, std::error_code>
FileManager::loadFile(const std::string &filename) {
  // Get file info first
  std::error_code ec;
  if (!std::filesystem::exists(filename, ec) || ec) {
    return {nullptr,
            ec ? ec
               : std::make_error_code(std::errc::no_such_file_or_directory)};
  }

  auto fileSize = std::filesystem::file_size(filename, ec);
  if (ec) {
    return {nullptr, ec};
  }

  auto modTime = std::filesystem::last_write_time(filename, ec);
  if (ec) {
    return {nullptr, ec};
  }

  time_t modTimeT = std::chrono::duration_cast<std::chrono::seconds>(
                        modTime.time_since_epoch())
                        .count();

  ++stats.fileOpenCount;
  stats.bytesReadCount += fileSize;

  // Try memory mapping for large files
  if (memoryMappingEnabled && fileSize >= memoryMappingThreshold) {
    // For simplicity, fall back to regular file reading
    // A full implementation would use platform-specific memory mapping
  }

  // Regular file reading
  std::ifstream file(filename, std::ios::binary);
  if (!file) {
    return {nullptr, std::make_error_code(std::errc::io_error)};
  }

  // Allocate buffer with extra space for null terminator
  auto buffer = std::make_unique<char[]>(fileSize + 1);

  file.read(buffer.get(), fileSize);
  if (file.gcount() != static_cast<std::streamsize>(fileSize)) {
    return {nullptr, std::make_error_code(std::errc::io_error)};
  }

  // Null-terminate the buffer
  buffer[fileSize] = '\0';

  InternedString internedFilename = interner.intern(filename);
  auto entry = std::make_shared<FileEntry>(internedFilename, std::move(buffer),
                                           fileSize, modTimeT);

  return {entry, {}};
}

void FileManager::evictIfNeeded() {
  // Simple LRU eviction based on cache size
  // In a production implementation, this would be more sophisticated
  if (maxCacheSize == SIZE_MAX) {
    return; // No limit
  }

  size_t currentSize = 0;
  for (const auto &[filename, entry] : fileCache) {
    currentSize += entry->getSize();
  }

  if (currentSize <= maxCacheSize) {
    return; // Under limit
  }

  // Simple eviction: remove files until we're under the limit
  // A better implementation would track access times for true LRU
  auto it = fileCache.begin();
  while (it != fileCache.end() && currentSize > maxCacheSize) {
    currentSize -= it->second->getSize();
    it = fileCache.erase(it);
    --stats.fileCacheCount;
  }
}

std::string FileManager::normalizeFilename(const std::string &filename) const {
  std::error_code ec;
  auto canonical = std::filesystem::canonical(filename, ec);
  if (ec) {
    // If canonicalization fails, just return the original path
    return filename;
  }

  return canonical.string();
}

} // namespace ml
