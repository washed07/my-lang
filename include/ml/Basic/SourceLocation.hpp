#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace ml {

class SourceManager;

/**
 * \brief A unique identifier for a source file.
 * \details Encapsulates a unique ID for a source file within the
 * \ref SourceManager. Used to reference files efficiently.
 * \see SourceManager for usage cases.
 */
class FileID {
public:
  FileID() : id(0) {}

  /**
   * \brief Check if the FileID is valid.
   * \return True if valid, false otherwise.
   * \see isInvalid()
   */
  bool isValid() const { return id != 0; }

  /**
   * \brief Check if the FileID is invalid.
   * \return True if invalid, false otherwise.
   * \see isValid()
   */
  bool isInvalid() const { return id == 0; }

  /**
   * \brief Get an invalid FileID.
   * \return An invalid FileID instance. \c FileID()
   */
  static FileID getInvalidID() { return FileID(); }

  bool operator==(const FileID &RHS) const { return id == RHS.id; }
  bool operator!=(const FileID &RHS) const { return id != RHS.id; }
  bool operator<(const FileID &RHS) const { return id < RHS.id; }

  /**
   * \brief Get the hash value of this FileID.
   * \return The hash value as a uint32_t.
   */
  uint32_t getHashValue() const { return id; }

private:
  friend class SourceManager;
  explicit FileID(uint32_t id) : id(id) {}

  /**
   * \brief The unique identifier for the file.
   */
  uint32_t id;
};

/**
 * \brief A specific location in source code.
 * \details Encapsulates a specific location within a source file,
 * providing an efficient 32-bit representation that can represent locations
 * in files up to 4GB in size.
 * \see SourceManager for usage cases.
 */
class SourceLocation {
public:
  SourceLocation() : id(0) {}

  /**
   * \brief Check if the SourceLocation is valid.
   * \return True if valid, false otherwise.
   * \see isInvalid()
   */
  bool isValid() const { return id != 0; }

  /**
   * \brief Check if the SourceLocation is invalid.
   * \return True if invalid, false otherwise.
   * \see isValid()
   */
  bool isInvalid() const { return id == 0; }

  /**
   * \brief Get an invalid SourceLocation.
   * \return An invalid SourceLocation instance. \c SourceLocation()
   */
  static SourceLocation getInvalidLoc() { return SourceLocation(); }

  bool operator==(const SourceLocation &RHS) const { return id == RHS.id; }
  bool operator!=(const SourceLocation &RHS) const { return id != RHS.id; }
  bool operator<(const SourceLocation &RHS) const { return id < RHS.id; }

  /**
   * \brief Get the hash value of this SourceLocation.
   * \return The hash value as a uint32_t.
   */
  uint32_t getHashValue() const { return id; }

  /**
   * \brief Get the raw encoding of this SourceLocation.
   * \return The raw encoding as a uint32_t.
   * \see getFromRawEncoding(uint32_t) for the inverse operation.
   */
  uint32_t getRawEncoding() const { return id; }

  /**
   * \brief Create a SourceLocation from a raw encoding.
   * \param encoding The raw encoding as a uint32_t.
   * \return The corresponding SourceLocation.
   * \see getRawEncoding() for the inverse operation.
   */
  static SourceLocation getFromRawEncoding(uint32_t encoding) {
    SourceLocation loc;
    loc.id = encoding;
    return loc;
  }

  /**
   * \brief Print the source location using the provided SourceManager.
   * \param os The output stream to print to.
   * \see printToString(const SourceManager &) const for string representation.
   */
  void print(std::ostream &os, const SourceManager &SM) const;

  /**
   * \brief Get a string representation of the source location.
   * \param SM The SourceManager to use for context.
   * \return A string representing the source location.
   * \see print(std::ostream &, const SourceManager &) const for printing.
   */
  std::string printToString(const SourceManager &SM) const;

private:
  friend class SourceManager;
  explicit SourceLocation(uint32_t id) : id(id) {}

  /**
   * \brief The encoded source location.
   */
  uint32_t id;
};

/**
 * \brief A range between two source locations.
 * \details Represents a range in source code defined by a starting and ending
 * location.
 * \see SourceLocation for individual location representation.
 */
class SourceRange {
public:
  SourceRange() = default;
  SourceRange(SourceLocation loc) : begin(loc), end(loc) {}
  SourceRange(SourceLocation begin, SourceLocation end)
      : begin(begin), end(end) {}

  /**
   * \brief Get the beginning location of the range.
   * \return The starting SourceLocation.
   */
  SourceLocation getBegin() const { return begin; }

  /**
   * \brief Get the ending location of the range.
   * \return The ending SourceLocation.
   */
  SourceLocation getEnd() const { return end; }

  /**
   * \brief Set the beginning location of the range.
   * \param loc The new starting SourceLocation.
   */
  void setBegin(SourceLocation loc) { begin = loc; }

  /**
   * \brief Set the ending location of the range.
   * \param loc The new ending SourceLocation.
   */
  void setEnd(SourceLocation loc) { end = loc; }

  /**
   * \brief Check if the source range is valid.
   * \return True if both begin and end locations are valid.
   * \see isInvalid()
   */
  bool isValid() const { return begin.isValid() && end.isValid(); }

  /**
   * \brief Check if the source range is invalid.
   * \return True if either begin or end locations are invalid.
   * \see isValid()
   */
  bool isInvalid() const { return !isValid(); }

  bool operator==(const SourceRange &RHS) const {
    return begin == RHS.begin && end == RHS.end;
  }
  bool operator!=(const SourceRange &RHS) const { return !(*this == RHS); }

private:
  /**
   * \brief The locations defining the range.
   */
  SourceLocation begin, end;
};

/**
 * \struct FullSourceLoc SourceLocation.hpp "ml/Basic/SourceLocation.hpp"
 * \brief A source location with additional context from SourceManager.
 * \details Combines a \ref SourceLocation with a reference to the
 * \ref SourceManager to provide enriched information about the location,
 * such as filename, line number, and column number.
 * \see SourceLocation for basic location representation.
 * \see SourceManager for context management.
 */
struct FullSourceLoc {

  /**
   * \brief The underlying source location.
   */
  SourceLocation location;

  /**
   * \brief The associated SourceManager.
   */
  const SourceManager *srcMgr;

  FullSourceLoc() : srcMgr(nullptr) {}
  FullSourceLoc(SourceLocation loc, const SourceManager &sm)
      : location(loc), srcMgr(&sm) {}

  /**
   * \brief Check if the FullSourceLoc is valid.
   * \return True if both the location and SourceManager are valid.
   */
  bool isValid() const { return location.isValid() && srcMgr; }

  /**
   * \brief Get the FileID of the source location.
   * \return The FileID corresponding to the location.
   */
  FileID getFileID() const;

  /**
   * \brief Get the file offset of the source location.
   * \return The byte offset within the file.
   */
  uint32_t getFileOffset() const;

  /**
   * \brief Get the associated FileEntry.
   * \return A pointer to the FileEntry for the location.
   */
  uint32_t getLineNumber() const;

  /**
   * \brief Get the column number of the source location.
   * \return The column number. 1-based
   */
  uint32_t getColumnNumber() const;

  /**
   * \brief Get the character data at the source location.
   * \return A pointer to the character data.
   */
  const char *getCharacterData() const;

  /**
   * \brief Get the filename for the source location.
   * \return The filename as a string.
   */
  std::string getFilename() const;
};

} // namespace ml