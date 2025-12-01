#include "ml/Managers/SourceManager.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace ml {

// Fast lookup cache for frequently accessed locations
struct LocationCache {
  SourceLocation last_location;
  uint32_t last_line = 0;
  uint32_t last_column = 0;
  const char *last_line_start = nullptr;
  FileID last_file_id;

  void invalidate() {
    last_location = SourceLocation::getInvalidLoc();
    last_file_id = FileID::getInvalidID();
    last_line = 0;
    last_column = 0;
    last_line_start = nullptr;
  }
};

thread_local LocationCache g_location_cache;

// FullSourceLoc implementations
FileID FullSourceLoc::getFileID() const {
  return srcMgr ? srcMgr->getFileID(location) : FileID::getInvalidID();
}

uint32_t FullSourceLoc::getFileOffset() const {
  return srcMgr ? srcMgr->getFileOffset(location) : 0;
}

uint32_t FullSourceLoc::getLineNumber() const {
  return srcMgr ? srcMgr->getLineNumber(location) : 0;
}

uint32_t FullSourceLoc::getColumnNumber() const {
  return srcMgr ? srcMgr->getColumnNumber(location) : 0;
}

const char *FullSourceLoc::getCharacterData() const {
  return srcMgr ? srcMgr->getCharacterData(location) : nullptr;
}

std::string FullSourceLoc::getFilename() const {
  return srcMgr ? srcMgr->getFilename(location).toString() : std::string();
}

// SourceLocation implementations
void SourceLocation::print(std::ostream &os, const SourceManager &SM) const {
  if (isInvalid()) {
    os << "<invalid loc>";
    return;
  }

  auto filename = SM.getFilenameView(*this);
  uint32_t line = SM.getLineNumber(*this);
  uint32_t col = SM.getColumnNumber(*this);

  os << filename << ":" << line << ":" << col;
}

std::string SourceLocation::printToString(const SourceManager &SM) const {
  std::ostringstream os;
  print(os, SM);
  return os.str();
}

// SourceManager implementations
SourceManager::SourceManager(FileManager &fileMgr) : fileMgr(fileMgr) {
  // Reserve space for typical project sizes
  loadedFiles.reserve(256);
  filenameToFileID.reserve(256);

  // Initialize location cache
  g_location_cache.invalidate();
}

SourceManager::~SourceManager() = default;

SourceManager::SourceManager(SourceManager &&other) noexcept
    : fileMgr(other.fileMgr), loadedFiles(std::move(other.loadedFiles)),
      filenameToFileID(std::move(other.filenameToFileID)),
      nextLocationID(other.nextLocationID.load()), stats(other.stats) {}

SourceManager &SourceManager::operator=(SourceManager &&other) noexcept {
  if (this != &other) {
    loadedFiles = std::move(other.loadedFiles);
    filenameToFileID = std::move(other.filenameToFileID);
    nextLocationID = other.nextLocationID.load();
    stats = other.stats;
  }
  return *this;
}

FileID SourceManager::createFileID(const std::string &filename) {
  auto [fid, error] = createFileIDWithError(filename);
  return fid;
}

std::pair<FileID, std::error_code>
SourceManager::createFileIDWithError(const std::string &filename) {
  // Load the file first to get the FileEntry with interned filename
  auto [entry, error] = fileMgr.getFileWithError(filename);
  if (error) {
    return {FileID::getInvalidID(), error};
  }

  // Fast path: check if file is already loaded using the interned filename
  InternedString internedFilename = entry->getFilename();
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto it = filenameToFileID.find(internedFilename);
    if (it != filenameToFileID.end()) {
      return {it->second, {}};
    }
  }

  return {createFileIDImpl(entry), {}};
}

FileID SourceManager::createFileID(std::shared_ptr<FileEntry> entry) {
  return createFileIDImpl(entry);
}

FileID SourceManager::createFileIDImpl(std::shared_ptr<FileEntry> entry) {
  std::lock_guard<std::mutex> lock(stateMutex);

  InternedString filename = entry->getFilename();

  // Check if already loaded (double-check with lock)
  auto it = filenameToFileID.find(filename);
  if (it != filenameToFileID.end()) {
    return it->second;
  }

  // Calculate offset for this file in global space
  uint32_t offset = getNextLocationID();

  // Create file info
  FileInfo info(entry, offset);

  // Add to our collections
  loadedFiles.push_back(std::move(info));
  FileID fid(loadedFiles.size()); // 1-based indexing

  filenameToFileID[filename] = fid;

  // Update stats
  ++stats.fileLoadCount;
  stats.sourceSize += entry->getSize();

  return fid;
}

SourceLocation SourceManager::getLocForStartOfFile(FileID fid) const {
  if (fid.isInvalid() || fid.getHashValue() == 0 ||
      fid.getHashValue() > loadedFiles.size()) {
    return SourceLocation::getInvalidLoc();
  }

  const FileInfo &info = loadedFiles[fid.getHashValue() - 1];
  return SourceLocation(info.offset);
}

SourceLocation SourceManager::getLocForEndOfFile(FileID fid) const {
  if (fid.isInvalid() || fid.getHashValue() == 0 ||
      fid.getHashValue() > loadedFiles.size()) {
    return SourceLocation::getInvalidLoc();
  }

  const FileInfo &info = loadedFiles[fid.getHashValue() - 1];
  return SourceLocation(info.offset +
                        static_cast<uint32_t>(info.entry->getSize()));
}

SourceLocation SourceManager::getLocForFileOffset(FileID fid,
                                                  uint32_t offset) const {
  if (fid.isInvalid() || fid.getHashValue() == 0 ||
      fid.getHashValue() > loadedFiles.size()) {
    return SourceLocation::getInvalidLoc();
  }

  const FileInfo &info = loadedFiles[fid.getHashValue() - 1];
  if (offset > info.entry->getSize()) {
    return SourceLocation::getInvalidLoc();
  }

  return SourceLocation(info.offset + offset);
}

FileID SourceManager::getFileID(SourceLocation loc) const {
  if (loc.isInvalid()) {
    return FileID::getInvalidID();
  }

  uint32_t locID = loc.getRawEncoding();

  // Fast path: check cache for recently accessed location
  if (g_location_cache.last_location.isValid() &&
      g_location_cache.last_file_id.isValid()) {

    const FileInfo &cached_info =
        loadedFiles[g_location_cache.last_file_id.getHashValue() - 1];
    uint32_t file_start = cached_info.offset;
    uint32_t file_end =
        file_start + static_cast<uint32_t>(cached_info.entry->getSize());

    if (locID >= file_start && locID <= file_end) {
      return g_location_cache.last_file_id;
    }
  }

  // Binary search to find the file containing this location
  auto it = std::upper_bound(
      loadedFiles.begin(), loadedFiles.end(), locID,
      [](uint32_t loc, const FileInfo &info) { return loc < info.offset; });

  if (it == loadedFiles.begin()) {
    return FileID::getInvalidID();
  }

  --it;

  // Check if the location is within this file's range
  uint32_t fileStart = it->offset;
  uint32_t fileEnd = fileStart + static_cast<uint32_t>(it->entry->getSize());

  if (locID < fileStart || locID > fileEnd) {
    return FileID::getInvalidID();
  }

  // Convert iterator to FileID and cache result
  size_t index = std::distance(loadedFiles.begin(), it);
  FileID result = FileID(static_cast<uint32_t>(index + 1));

  // Update cache
  g_location_cache.last_location = loc;
  g_location_cache.last_file_id = result;

  return result;
}

uint32_t SourceManager::getFileOffset(SourceLocation loc) const {
  FileID fid = getFileID(loc);
  if (fid.isInvalid()) {
    return 0;
  }

  const FileInfo &info = loadedFiles[fid.getHashValue() - 1];
  return loc.getRawEncoding() - info.offset;
}

const FileEntry *SourceManager::getFileEntry(SourceLocation loc) const {
  FileID fid = getFileID(loc);
  return getFileEntry(fid);
}

const FileEntry *SourceManager::getFileEntry(FileID fid) const {
  if (fid.isInvalid() || fid.getHashValue() == 0 ||
      fid.getHashValue() > loadedFiles.size()) {
    return nullptr;
  }

  return loadedFiles[fid.getHashValue() - 1].entry.get();
}

InternedString SourceManager::getFilename(SourceLocation loc) const {
  const FileEntry *entry = getFileEntry(loc);
  return entry ? entry->getFilename() : InternedString();
}

InternedString SourceManager::getFilename(FileID fid) const {
  const FileEntry *entry = getFileEntry(fid);
  return entry ? entry->getFilename() : InternedString();
}

std::string_view SourceManager::getFilenameView(SourceLocation loc) const {
  const FileEntry *entry = getFileEntry(loc);
  return entry ? entry->getFilenameView() : std::string_view();
}

std::string_view SourceManager::getFilenameView(FileID fid) const {
  const FileEntry *entry = getFileEntry(fid);
  return entry ? entry->getFilenameView() : std::string_view();
}

uint32_t SourceManager::getLineNumber(SourceLocation loc) const {
  if (loc.isInvalid()) {
    return 0;
  }

  // Fast path: check if we can use cached line info
  if (g_location_cache.last_location.isValid() &&
      g_location_cache.last_file_id.isValid() &&
      g_location_cache.last_line > 0) {

    // If it's the same location, return cached result
    if (loc == g_location_cache.last_location) {
      return g_location_cache.last_line;
    }

    // If it's in the same file and on the same line, we can compute quickly
    FileID fid = getFileID(loc);
    if (fid == g_location_cache.last_file_id &&
        g_location_cache.last_line_start) {
      uint32_t offset = getFileOffset(loc);
      const FileInfo &info = loadedFiles[fid.getHashValue() - 1];

      // Check if still on the same line
      const char *current_pos = info.entry->getBufferStart() + offset;
      const char *line_start = g_location_cache.last_line_start;

      // Scan forward to see if we hit a newline
      const char *scan = line_start;
      while (scan <= current_pos && *scan != '\n' && *scan != '\r') {
        ++scan;
      }

      if (scan > current_pos) {
        // Still on same line
        return g_location_cache.last_line;
      }
    }
  }

  FileID fid = getFileID(loc);
  if (fid.isInvalid()) {
    return 0;
  }

  computeLineOffsets(fid);

  const FileInfo &info = loadedFiles[fid.getHashValue() - 1];
  uint32_t offset = getFileOffset(loc);

  uint32_t line = findLineNumber(info.lineOffsets, offset);

  // Update cache
  g_location_cache.last_location = loc;
  g_location_cache.last_file_id = fid;
  g_location_cache.last_line = line;

  // Cache line start position for next lookup
  if (!info.lineOffsets.empty() && line > 0 &&
      line <= info.lineOffsets.size()) {
    uint32_t line_offset = (line == 1) ? 0 : info.lineOffsets[line - 2] + 1;
    g_location_cache.last_line_start =
        info.entry->getBufferStart() + line_offset;
  }

  return line;
}

uint32_t SourceManager::getColumnNumber(SourceLocation loc) const {
  if (loc.isInvalid()) {
    return 0;
  }

  // Fast path: check cache
  if (g_location_cache.last_location.isValid() &&
      g_location_cache.last_file_id.isValid() &&
      g_location_cache.last_column > 0) {

    if (loc == g_location_cache.last_location) {
      return g_location_cache.last_column;
    }
  }

  FileID fid = getFileID(loc);
  if (fid.isInvalid()) {
    return 0;
  }

  computeLineOffsets(fid);

  const FileInfo &info = loadedFiles[fid.getHashValue() - 1];
  uint32_t offset = getFileOffset(loc);

  uint32_t line = findLineNumber(info.lineOffsets, offset);
  if (line == 0 || line > info.lineOffsets.size()) {
    return 0;
  }

  uint32_t lineStart = info.lineOffsets[line - 1];
  uint32_t column = offset - lineStart + 1; // 1-based column numbers

  // Update cache
  g_location_cache.last_location = loc;
  g_location_cache.last_file_id = fid;
  g_location_cache.last_column = column;

  return column;
}

std::pair<uint32_t, uint32_t>
SourceManager::getLineAndColumn(SourceLocation loc) const {
  FileID fid = getFileID(loc);
  if (fid.isInvalid()) {
    return {0, 0};
  }

  computeLineOffsets(fid);

  const FileInfo &info = loadedFiles[fid.getHashValue() - 1];
  uint32_t offset = getFileOffset(loc);

  uint32_t line = findLineNumber(info.lineOffsets, offset);
  if (line == 0 || line > info.lineOffsets.size()) {
    return {0, 0};
  }

  uint32_t lineStart = info.lineOffsets[line - 1];
  uint32_t column = offset - lineStart + 1; // 1-based column numbers

  return {line, column};
}

const char *SourceManager::getCharacterData(SourceLocation loc) const {
  const FileEntry *entry = getFileEntry(loc);
  if (!entry) {
    return nullptr;
  }

  uint32_t offset = getFileOffset(loc);
  if (offset >= entry->getSize()) {
    return nullptr;
  }

  return entry->getBufferStart() + offset;
}

std::string SourceManager::getSourceText(SourceRange range) const {
  return getSourceText(range.getBegin(), range.getEnd());
}

std::string SourceManager::getSourceText(SourceLocation start,
                                         SourceLocation end) const {
  if (start.isInvalid() || end.isInvalid()) {
    return std::string();
  }

  // Make sure both locations are in the same file
  FileID startFID = getFileID(start);
  FileID endFID = getFileID(end);

  if (startFID != endFID) {
    return std::string(); // Cross-file ranges not supported
  }

  const char *startPtr = getCharacterData(start);
  const char *endPtr = getCharacterData(end);

  if (!startPtr || !endPtr || startPtr > endPtr) {
    return std::string();
  }

  return std::string(startPtr, endPtr);
}

size_t SourceManager::getSourceLength(SourceLocation start,
                                      SourceLocation end) const {
  if (start.isInvalid() || end.isInvalid()) {
    return 0;
  }

  FileID startFID = getFileID(start);
  FileID endFID = getFileID(end);

  if (startFID != endFID) {
    return 0; // Cross-file ranges not supported
  }

  uint32_t startOffset = getFileOffset(start);
  uint32_t endOffset = getFileOffset(end);

  return endOffset >= startOffset ? (endOffset - startOffset) : 0;
}

bool SourceManager::isValidSourceLocation(SourceLocation loc) const {
  return loc.isValid() && getFileID(loc).isValid();
}

bool SourceManager::isBeforeInSourceOrder(SourceLocation lhs,
                                          SourceLocation rhs) const {
  if (lhs.isInvalid() || rhs.isInvalid()) {
    return false;
  }

  return lhs.getRawEncoding() < rhs.getRawEncoding();
}

SourceLocation SourceManager::advanceSourceLocation(SourceLocation loc,
                                                    uint32_t numChars) const {
  if (loc.isInvalid()) {
    return SourceLocation::getInvalidLoc();
  }

  FileID fid = getFileID(loc);
  if (fid.isInvalid()) {
    return SourceLocation::getInvalidLoc();
  }

  const FileInfo &info = loadedFiles[fid.getHashValue() - 1];
  uint32_t offset = getFileOffset(loc);

  if (offset + numChars > info.entry->getSize()) {
    return SourceLocation::getInvalidLoc();
  }

  return SourceLocation(loc.getRawEncoding() + numChars);
}

void SourceManager::printStats() const {
  std::lock_guard<std::mutex> lock(stateMutex);

  std::cout << "SourceManager Statistics:\n";
  std::cout << "  Files loaded: " << stats.fileLoadCount << "\n";
  std::cout << "  Locations created: " << stats.locationCreateCount << "\n";
  std::cout << "  Line computations: " << stats.lineComputationCount << "\n";
  std::cout << "  Total source size: " << stats.sourceSize << " bytes\n";
}

SourceManagerStats SourceManager::getStats() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return stats;
}

void SourceManager::clearCache() {
  std::lock_guard<std::mutex> lock(stateMutex);

  // Clear line offset caches
  for (auto &info : loadedFiles) {
    info.lineOffsets.clear();
    info.lineOffsetsComputed = false;
  }
}

void SourceManager::computeLineOffsets(FileID fid) const {
  if (fid.isInvalid() || fid.getHashValue() == 0 ||
      fid.getHashValue() > loadedFiles.size()) {
    return;
  }

  FileInfo &info = const_cast<FileInfo &>(loadedFiles[fid.getHashValue() - 1]);

  if (info.lineOffsetsComputed) {
    return;
  }

  ++stats.lineComputationCount;

  // Compute line start offsets
  const char *data = info.entry->getBufferStart();
  size_t size = info.entry->getSize();

  info.lineOffsets.clear();
  info.lineOffsets.reserve(size / 40 + 16); // Estimate ~40 chars per line
  info.lineOffsets.push_back(0);            // Line 1 starts at offset 0

  // SIMD-optimized newline scanning
#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
  const __m256i newline = _mm256_set1_epi8('\n');
  size_t i = 0;

  // Process 32 bytes at a time with AVX2
  while (i + 32 <= size) {
    __m256i chunk =
        _mm256_loadu_si256(reinterpret_cast<const __m256i *>(data + i));
    __m256i cmp = _mm256_cmpeq_epi8(chunk, newline);
    uint32_t mask = _mm256_movemask_epi8(cmp);

    if (mask != 0) {
      // Found newlines, process each bit
      for (int bit = 0; bit < 32; ++bit) {
        if (mask & (1u << bit)) {
          info.lineOffsets.push_back(static_cast<uint32_t>(i + bit + 1));
        }
      }
    }
    i += 32;
  }

  // Handle remaining bytes
  while (i < size) {
    if (data[i] == '\n') {
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 1));
    }
    ++i;
  }

#elif defined(__SSE2__) || (defined(_MSC_VER) && defined(__SSE2__))
  const __m128i newline = _mm_set1_epi8('\n');
  size_t i = 0;

  // Process 16 bytes at a time with SSE2
  while (i + 16 <= size) {
    __m128i chunk =
        _mm_loadu_si128(reinterpret_cast<const __m128i *>(data + i));
    __m128i cmp = _mm_cmpeq_epi8(chunk, newline);
    uint32_t mask = _mm_movemask_epi8(cmp);

    if (mask != 0) {
      // Found newlines, process each bit
      for (int bit = 0; bit < 16; ++bit) {
        if (mask & (1u << bit)) {
          info.lineOffsets.push_back(static_cast<uint32_t>(i + bit + 1));
        }
      }
    }
    i += 16;
  }

  // Handle remaining bytes
  while (i < size) {
    if (data[i] == '\n') {
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 1));
    }
    ++i;
  }

#else
  // Fallback scalar implementation with unrolled loop
  size_t i = 0;
  while (i + 8 <= size) {
    // Unroll loop for better performance
    if (data[i] == '\n')
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 1));
    if (data[i + 1] == '\n')
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 2));
    if (data[i + 2] == '\n')
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 3));
    if (data[i + 3] == '\n')
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 4));
    if (data[i + 4] == '\n')
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 5));
    if (data[i + 5] == '\n')
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 6));
    if (data[i + 6] == '\n')
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 7));
    if (data[i + 7] == '\n')
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 8));
    i += 8;
  }

  // Handle remaining bytes
  while (i < size) {
    if (data[i] == '\n') {
      info.lineOffsets.push_back(static_cast<uint32_t>(i + 1));
    }
    ++i;
  }
#endif

  info.lineOffsetsComputed = true;
}

uint32_t SourceManager::findLineNumber(const std::vector<uint32_t> &lineOffsets,
                                       uint32_t offset) const {
  if (lineOffsets.empty()) {
    return 0;
  }

  // Binary search for the line containing the offset
  auto it = std::upper_bound(lineOffsets.begin(), lineOffsets.end(), offset);

  if (it == lineOffsets.begin()) {
    return 1; // Before first line somehow, return line 1
  }

  --it;
  return static_cast<uint32_t>(std::distance(lineOffsets.begin(), it) + 1);
}

uint32_t SourceManager::getNextLocationID() {
  return nextLocationID.fetch_add(1);
}

} // namespace ml