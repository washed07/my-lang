#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace ml {

class SourceManager;

/// Represents a unique identifier for a file in the source manager.
class FileID {
public:
  FileID() : id(0) {}

  bool isValid() const { return id != 0; }
  bool isInvalid() const { return id == 0; }

  static FileID getInvalidID() { return FileID(); }

  bool operator==(const FileID &RHS) const { return id == RHS.id; }
  bool operator!=(const FileID &RHS) const { return id != RHS.id; }
  bool operator<(const FileID &RHS) const { return id < RHS.id; }

  uint32_t getHashValue() const { return id; }

private:
  friend class SourceManager;
  explicit FileID(uint32_t id) : id(id) {}

  uint32_t id;
};

/// Encodes a location in the source.
/// This is an efficient 32-bit representation that can represent locations
/// in files up to 4GB in size.
class SourceLocation {
public:
  SourceLocation() : id(0) {}

  bool isValid() const { return id != 0; }
  bool isInvalid() const { return id == 0; }

  static SourceLocation getInvalidLoc() { return SourceLocation(); }

  bool operator==(const SourceLocation &RHS) const { return id == RHS.id; }
  bool operator!=(const SourceLocation &RHS) const { return id != RHS.id; }
  bool operator<(const SourceLocation &RHS) const { return id < RHS.id; }

  uint32_t getHashValue() const { return id; }

  /// Get the raw encoding of this source location.
  uint32_t getRawEncoding() const { return id; }

  /// Create a source location from a raw encoding.
  static SourceLocation getFromRawEncoding(uint32_t encoding) {
    SourceLocation loc;
    loc.id = encoding;
    return loc;
  }

  void print(std::ostream &os, const SourceManager &SM) const;
  std::string printToString(const SourceManager &SM) const;

private:
  friend class SourceManager;
  explicit SourceLocation(uint32_t id) : id(id) {}

  uint32_t id;
};

/// Represents a range of source locations.
class SourceRange {
public:
  SourceRange() = default;
  SourceRange(SourceLocation loc) : begin(loc), end(loc) {}
  SourceRange(SourceLocation begin, SourceLocation end)
      : begin(begin), end(end) {}

  SourceLocation getBegin() const { return begin; }
  SourceLocation getEnd() const { return end; }

  void setBegin(SourceLocation loc) { begin = loc; }
  void setEnd(SourceLocation loc) { end = loc; }

  bool isValid() const { return begin.isValid() && end.isValid(); }
  bool isInvalid() const { return !isValid(); }

  bool operator==(const SourceRange &RHS) const {
    return begin == RHS.begin && end == RHS.end;
  }
  bool operator!=(const SourceRange &RHS) const { return !(*this == RHS); }

private:
  SourceLocation begin, end;
};

/// Full source location information for a source location.
struct FullSourceLoc {
  SourceLocation location;
  const SourceManager *srcMgr;

  FullSourceLoc() : srcMgr(nullptr) {}
  FullSourceLoc(SourceLocation loc, const SourceManager &sm)
      : location(loc), srcMgr(&sm) {}

  bool isValid() const { return location.isValid() && srcMgr; }

  FileID getFileID() const;
  uint32_t getFileOffset() const;
  uint32_t getLineNumber() const;
  uint32_t getColumnNumber() const;
  const char *getCharacterData() const;

  std::string getFilename() const;
};

} // namespace ml