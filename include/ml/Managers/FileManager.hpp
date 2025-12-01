#pragma once

#include "ml/Basic/StringInterner.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace ml {

/// Represents the contents of a file in memory.
class FileEntry {
public:
  FileEntry(InternedString filename, std::unique_ptr<char[]> data, size_t size,
            time_t modTime)
      : filename(filename), data(std::move(data)), size(size), modTime(modTime),
        refCount(1) {}

  ~FileEntry() = default;

  // Non-copyable but movable
  FileEntry(const FileEntry &) = delete;
  FileEntry &operator=(const FileEntry &) = delete;

  InternedString getFilename() const { return filename; }
  std::string_view getFilenameView() const { return filename.str(); }
  const char *getData() const { return data.get(); }
  size_t getSize() const { return size; }
  time_t getModificationTime() const { return modTime; }

  /// Get a null-terminated view of the buffer.
  /// Note: The buffer is guaranteed to have a null terminator.
  const char *getBufferStart() const { return data.get(); }
  const char *getBufferEnd() const { return data.get() + size; }

  /// Reference counting for shared ownership
  void addRef() { ++refCount; }
  bool removeRef() { return --refCount == 0; }
  uint32_t getRefCount() const { return refCount; }

private:
  InternedString filename;
  std::unique_ptr<char[]> data;
  size_t size;
  time_t modTime;
  std::atomic<uint32_t> refCount;
};

/// Statistics about file operations
struct FileManagerStats {
  size_t fileOpenCount = 0;
  size_t fileCacheCount = 0;
  size_t bytesReadCount = 0;
  size_t cacheHitCount = 0;
  size_t cacheMissCount = 0;
};

/// Manages file loading, caching, and memory mapping for the compiler.
/// This class is thread-safe and designed to handle large numbers of files
/// efficiently.
class FileManager {
public:
  explicit FileManager(StringInterner &interner);
  ~FileManager();

  // Non-copyable but movable
  FileManager(const FileManager &) = delete;
  FileManager &operator=(const FileManager &) = delete;
  FileManager(FileManager &&) noexcept;
  FileManager &operator=(FileManager &&) noexcept;

  /// Get a file entry for the specified filename.
  /// Returns nullptr if the file cannot be opened or read.
  /// The returned pointer remains valid until the FileManager is destroyed
  /// or clearCache() is called.
  std::shared_ptr<FileEntry> getFile(const std::string &filename);

  /// Get a file entry, with error information if it fails.
  std::pair<std::shared_ptr<FileEntry>, std::error_code>
  getFileWithError(const std::string &filename);

  /// Check if a file exists without loading it.
  bool fileExists(const std::string &filename) const;

  /// Get the size of a file without loading it.
  std::pair<size_t, std::error_code>
  getFileSize(const std::string &filename) const;

  /// Get file modification time.
  std::pair<time_t, std::error_code>
  getFileModTime(const std::string &filename) const;

  /// Clear the file cache. This will invalidate all previously returned
  /// FileEntry pointers.
  void clearCache();

  /// Remove a specific file from the cache.
  void removeFromCache(const std::string &filename);

  /// Set the maximum cache size in bytes. Files will be evicted when this limit
  /// is exceeded.
  void setMaxCacheSize(size_t maxSize) { maxCacheSize = maxSize; }
  size_t getMaxCacheSize() const { return maxCacheSize; }

  /// Get current cache size in bytes.
  size_t getCurrentCacheSize() const;

  /// Get statistics about file operations.
  FileManagerStats getStats() const;

  /// Enable or disable memory mapping for large files.
  void setMemoryMappingEnabled(bool enabled) { memoryMappingEnabled = enabled; }
  bool isMemoryMappingEnabled() const { return memoryMappingEnabled; }

  /// Set the threshold size for memory mapping (default: 64KB).
  void setMemoryMappingThreshold(size_t threshold) {
    memoryMappingThreshold = threshold;
  }
  size_t getMemoryMappingThreshold() const { return memoryMappingThreshold; }

private:
  /// Internal method to load a file from disk.
  std::pair<std::shared_ptr<FileEntry>, std::error_code>
  loadFile(const std::string &filename);

  /// Evict files from cache if needed to stay under the size limit.
  void evictIfNeeded();

  /// Normalize a filename to a canonical form for caching.
  std::string normalizeFilename(const std::string &filename) const;

  mutable std::mutex cacheMutex;
  std::unordered_map<InternedString, std::shared_ptr<FileEntry>,
                     InternedStringHash>
      fileCache;
  StringInterner &interner;

  // Configuration
  size_t maxCacheSize = SIZE_MAX; // No limit by default
  bool memoryMappingEnabled = true;
  size_t memoryMappingThreshold = 64 * 1024; // 64KB

  // Statistics
  mutable FileManagerStats stats;
};

} // namespace ml