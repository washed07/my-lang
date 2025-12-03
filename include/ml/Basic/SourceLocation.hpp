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
   * \brief Checks if the FileID is valid.
   * \return True if valid, false otherwise.
   * \note Invalid if ID is 0.
   */
  bool isValid() const { return id != 0; }

  /**
   * \brief Gets an invalid FileID.
   * \return An invalid FileID instance.
   * \note Equivalent to \c FileID()
   */
  static FileID getInvalid() { return FileID(); }

  /**
   * \brief Equality comparison.
   * \param RHS The other FileID to compare with.
   * \return True if IDs are equal, false otherwise.
   */
  bool operator==(const FileID &RHS) const { return id == RHS.id; }

  /**
   * \see operator==(const FileID &) const
   */
  bool operator!=(const FileID &RHS) const { return id != RHS.id; }

  /**
   * \brief Less-than comparison for ordering.
   * \param RHS The other FileID to compare with.
   * \return True if this ID is less than other, false otherwise.
   */
  bool operator<(const FileID &RHS) const { return id < RHS.id; }

  /**
   * \brief Gets the encoding.
   * \return The encoding as a uint32_t.
   */
  uint32_t getEncoding() const { return id; }

private:
  friend class SourceManager;
  explicit FileID(uint32_t id) : id(id) {}

  /**
   * \brief The unique identifier for the file.
   * \note Managed by \ref SourceManager.
   * \warning Do not modify directly.
   */
  uint32_t id;
};

/**
 * \brief A source location within a file.
 * \details Encapsulates a specific location within a source file,
 * providing an efficient 32-bit representation that can represent locations
 * in files up to 4GB in size.
 * \see SourceManager for management and retrieval of source locations.
 */
class SourceLocation {
public:
  SourceLocation() : encoding(0) {}

  /**
   * \brief Checks if the location is valid.
   * \return \c true if valid, \c false otherwise.
   * \note Invalid if \ref encoding is 0.
   */
  bool isValid() const { return encoding != 0; }

  /**
   * \brief Gets an invalid SourceLocation.
   * \return An invalid SourceLocation instance.
   * \note Equivalent to \c SourceLocation()
   */
  static SourceLocation getInvalid() { return SourceLocation(); }

  bool operator==(const SourceLocation &RHS) const {
    return encoding == RHS.encoding;
  }
  bool operator!=(const SourceLocation &RHS) const {
    return encoding != RHS.encoding;
  }
  bool operator<(const SourceLocation &RHS) const {
    return encoding < RHS.encoding;
  }

  /**
   * \brief Gets the encoding.
   * \return The encoding as a uint32_t.
   */
  uint32_t getEncoding() const { return encoding; }

  /**
   * \brief Prints the source location using the provided \ref SourceManager.
   * \param os The output stream to print to.
   * \see printToString(const SourceManager &) const for string representation.
   */
  void print(std::ostream &os, const SourceManager &SM) const;

  /**
   * \brief Gets a string representation of the source location.
   * \param SM The SourceManager to use for context.
   * \return A string representing the source location.
   * \see print(std::ostream &, const SourceManager &) const for printing.
   */
  std::string printToString(const SourceManager &SM) const;

private:
  friend class SourceManager;
  explicit SourceLocation(uint32_t id) : encoding(id) {}

  /**
   * \brief The encoded source location.
   * \note Managed by \ref SourceManager.
   * \warning Do not modify directly.
   */
  uint32_t encoding;
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
   * \brief Gets the beginning location of the range.
   * \return The starting \ref SourceLocation.
   */
  SourceLocation getBegin() const { return begin; }

  /**
   * \brief Gets the ending location of the range.
   * \return The ending \ref SourceLocation.
   */
  SourceLocation getEnd() const { return end; }

  /**
   * \brief Sets the beginning location of the range.
   * \param loc The new starting \ref SourceLocation.
   */
  void setBegin(SourceLocation loc) { begin = loc; }

  /**
   * \brief Sets the ending location of the range.
   * \param loc The new ending \ref SourceLocation.
   */
  void setEnd(SourceLocation loc) { end = loc; }

  /**
   * \brief Checks if the source range is valid.
   * \return True if both begin and end locations are valid.
   * \see SourceLocation::isValid() const
   */
  bool isValid() const { return begin.isValid() && end.isValid(); }

  bool operator==(const SourceRange &RHS) const {
    return begin == RHS.begin && end == RHS.end;
  }
  bool operator!=(const SourceRange &RHS) const { return !(*this == RHS); }

private:
  /**
   * \brief The location defining the range.
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
   * \brief Checks if the \ref FullSourceLoc is valid.
   * \return True if both the location and SourceManager are valid.
   */
  bool isValid() const { return location.isValid() && srcMgr; }

  /**
   * \brief Gets the \ref FileID of the \ref SourceLocation.
   * \return The \ref FileID corresponding.
   */
  FileID getFileID() const;

  /**
   * \brief Gets the file offset of the \ref SourceLocation.
   * \return The byte offset within the file.
   */
  uint32_t getFileOffset() const;

  /**
   * \brief Gets the associated \ref FileEntry.
   * \return A pointer to the \ref FileEntry for the location.
   */
  uint32_t getLineNumber() const;

  /**
   * \brief Gets the column number of the \ref SourceLocation.
   * \return The column number (1-based).
   */
  uint32_t getColumnNumber() const;

  /**
   * \brief Gets the character data at the \ref SourceLocation.
   * \return A pointer to the character data.
   */
  const char *getData() const;

  /**
   * \brief Gets the filename for the \ref SourceLocation.
   * \return The filename as a string.
   */
  std::string getFilename() const;
};

} // namespace ml