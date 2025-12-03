#pragma once

#include "ml/Basic/SourceLocation.hpp"
#include "ml/Basic/StringInterner.hpp"
#include "ml/Managers/FileManager.hpp"
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ml {

class FileEntry;

/// Information about a loaded file in the source manager.
struct FileInfo {
  std::shared_ptr<FileEntry> entry;
  uint32_t offset; // Offset of this file in the global source location space
  mutable std::vector<uint32_t> lineOffsets; // Cached line start positions
  mutable bool lineOffsetsComputed = false;

  FileInfo() : offset(0) {}
  FileInfo(std::shared_ptr<FileEntry> entry, uint32_t offset)
      : entry(std::move(entry)), offset(offset) {}
};

/// Statistics about source manager operations
struct SourceManagerStats {
  size_t fileLoadCount = 0;
  size_t locationCreateCount = 0;
  size_t lineComputationCount = 0;
  size_t sourceSize = 0;
};

/// Manages source code files and provides efficient source location services.
/// This class handles the mapping between SourceLocation objects and actual
/// file positions, line numbers, and column numbers.
///
/// The SourceManager is designed to:
/// - Handle arbitrarily large numbers of files efficiently
/// - Provide fast source location lookup and conversion
/// - Cache line/column computation results
/// - Work efficiently with the FileManager for file I/O
/// - Be thread-safe for read operations
class SourceManager {
public:
  explicit SourceManager(FileManager &fileMgr);
  ~SourceManager();

  // Non-copyable but movable
  SourceManager(const SourceManager &) = delete;
  SourceManager &operator=(const SourceManager &) = delete;
  SourceManager(SourceManager &&) noexcept;
  SourceManager &operator=(SourceManager &&) noexcept;

  /// Load a file and return its FileID.
  /// Returns an invalid FileID if the file cannot be loaded.
  FileID createFileID(const std::string &filename);

  /// Load a file with error information.
  std::pair<FileID, std::error_code>
  createFileIDWithError(const std::string &filename);

  /// Create a FileID for an already loaded FileEntry.
  FileID createFileID(std::shared_ptr<FileEntry> entry);

  /// Create a source location for a given file and byte offset.
  SourceLocation getLocForStartOfFile(FileID fid) const;
  SourceLocation getLocForEndOfFile(FileID fid) const;
  SourceLocation getLocForFileOffset(FileID fid, uint32_t offset) const;

  /// Get source location information.
  FileID getFileID(SourceLocation loc) const;
  uint32_t getFileOffset(SourceLocation loc) const;
  const FileEntry *getFileEntry(SourceLocation loc) const;
  const FileEntry *getFileEntry(FileID fid) const;

  /// Get the filename for a source location.
  InternedString getFilename(SourceLocation loc) const;
  InternedString getFilename(FileID fid) const;
  std::string_view getFilenameView(SourceLocation loc) const;
  std::string_view getFilenameView(FileID fid) const;

  /// Get line and column numbers (1-based).
  uint32_t getLineNumber(SourceLocation loc) const;
  uint32_t getColumnNumber(SourceLocation loc) const;
  std::pair<uint32_t, uint32_t> getLineAndColumn(SourceLocation loc) const;

  /// Get a pointer to the character data at a source location.
  const char *getCharacterData(SourceLocation loc) const;

  /// Get the source text for a given range.
  std::string getSourceText(SourceRange range) const;
  std::string getSourceText(SourceLocation start, SourceLocation end) const;

  /// Get the length of the source text between two locations.
  size_t getSourceLength(SourceLocation start, SourceLocation end) const;

  /// Check if a source location is valid.
  bool isValidSourceLocation(SourceLocation loc) const;

  /// Get expanded location information.
  FullSourceLoc getFullLoc(SourceLocation loc) const {
    return FullSourceLoc(loc, *this);
  }

  /// Utility functions for working with source ranges.
  bool isBeforeInSourceOrder(SourceLocation lhs, SourceLocation rhs) const;
  SourceLocation advanceSourceLocation(SourceLocation loc,
                                       uint32_t numChars) const;

  /// Debug and introspection functions.
  void printStats() const;
  SourceManagerStats getStats() const;

  /// Get the underlying file manager.
  FileManager &getFileManager() { return fileMgr; }
  const FileManager &getFileManager() const { return fileMgr; }

  /// Clear all cached data (but keep file entries alive through FileManager).
  void clearCache();

private:
  /// Compute line offsets for a file (cached after first computation).
  void computeLineOffsets(FileID fid) const;

  /// Find the line number for a given file offset using binary search.
  uint32_t findLineNumber(const std::vector<uint32_t> &lineOffsets,
                          uint32_t offset) const;

  /// Get the next available source location id.
  uint32_t getNextLocationID();

  /// Internal method to create a FileID.
  FileID createFileIDImpl(std::shared_ptr<FileEntry> entry);

  FileManager &fileMgr;

  // File tracking
  std::vector<FileInfo> loadedFiles;
  std::unordered_map<InternedString, FileID, InternedStringHash>
      filenameToFileID;

  // Source location allocation
  std::atomic<uint32_t> nextLocationID{1}; // 0 is reserved for invalid

  // Thread safety for modifications
  mutable std::mutex stateMutex;

  // Statistics
  mutable SourceManagerStats stats;

  // Constants
  static constexpr uint32_t invalidLocationID = 0;
};

/// Helper functions for working with source locations.
inline std::ostream &operator<<(std::ostream &os, SourceLocation loc) {
  return os << "SourceLocation(" << loc.getRawEncoding() << ")";
}

inline std::ostream &operator<<(std::ostream &os, FileID fid) {
  return os << "FileID(" << fid.get() << ")";
}

} // namespace ml