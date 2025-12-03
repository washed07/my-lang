#pragma once

#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ml {

class ArenaAllocator;

/**
 * \class InternedString StringInterner.hpp "ml/Basic/StringInterner.hpp"
 * \brief An interned string handle.
 * \details Represents a unique interned string stored in a
 * \ref StringInterner. Provides fast comparison via pointer equality
 * and access to the underlying string data.
 * \see StringInterner for managing interned strings.
 */
class InternedString {
public:
  InternedString() : ptr(nullptr) {}

  /**
   * \brief Converts to a C-style string.
   * \return A pointer to the null-terminated string.
   * \note Returns empty string "" if data is a \c nullptr value.
   * \see getData() for raw data access.
   */
  const char *toCStr() const { return ptr ? ptr : ""; }

  /**
   * \brief Gets the string data.
   * \return The underlying string data.
   * \note Returns \c nullptr if string is invalid.
   * \see toCStr() for safe access.
   */
  const char *getData() const { return ptr; }

  /**
   * \brief Converts to a \c std::string_view.
   * \return A \c std::string_view copy.
   * \note Returns empty view if data is a \c nullptr value.
   */
  std::string_view toStringView() const {
    return ptr ? std::string_view(ptr) : std::string_view();
  }

  /**
   * \brief Gets string length.
   * \return Length in characters.
   * \deprecated Use \ref length() instead.
   * \see length()
   */
  size_t size() const { return toStringView().size(); }

  /**
   * \brief Gets the string length.
   * \return Length in characters.
   */
  size_t length() const { return ptr ? strlen(ptr) : 0; }

  /**
   * \brief Checks if the string is empty.
   * \return \c true if \c nullptr or \c '\0' character, \c false otherwise.
   */
  bool isEmpty() const { return ptr == nullptr || *ptr == '\0'; }

  /**
   * \brief Pointer comparison operators for fast equality checks.
   */
  bool operator==(const InternedString &other) const {
    return ptr == other.ptr;
  }

  /**
   * \brief Pointer comparison operators for fast equality checks.
   */
  bool operator!=(const InternedString &other) const {
    return ptr != other.ptr;
  }

  /**
   * \brief Pointer comparison for ordering.
   */
  bool operator<(const InternedString &other) const { return ptr < other.ptr; }

  /**
   * \brief Compares with a \c std::string_view for equality.
   * \param other The \c std::string_view to compare with.
   * \return \c true if equal, \c false otherwise.
   * \see operator==(const InternedString &) const for pointer comparison.
   * \note Use pointer comparison for best performance when possible.
   */
  bool equals(std::string_view other) const { return toStringView() == other; }

  /**
   * \brief Checks if the string is valid.
   * \return \c false if \c nullptr, \c true otherwise.
   * \see isEmpty() for emptiness check.
   */
  bool isValid() const { return ptr != nullptr; }

  /**
   * \brief Gets the hash value of the string.
   * \return The hash value of the underlying data.
   * \note Hash is pointer-based for fast lookup in hash tables.
   */
  size_t getHashValue() const { return std::hash<const char *>{}(ptr); }

  /**
   * \brief Converts to a string.
   * \return A \c std::string copy.
   */
  std::string toString() const { return std::string(toStringView()); }

  /**
   * \brief Implicit conversion to \c std::string_view.
   * \return A \c std::string_view copy.
   */
  operator std::string_view() const { return toStringView(); }

private:
  friend class StringInterner;

  /**
   * \brief Constructs an \c InternedString from a raw pointer.
   * \param ptr The pointer to the interned string data.
   */
  explicit InternedString(const char *ptr) : ptr(ptr) {}

  /**
   * \brief A pointer to the interned string data.
   * \note Managed by \ref StringInterner.
   */
  const char *ptr;
};

/**
 * \struct StringInternerStats "StringInterner.hpp"
 * "ml/Basic/StringInterner.hpp"
 * \brief Statistics about the StringInterner.
 * \details Tracks various metrics such as number of interned strings,
 * memory usage, and collision counts.
 * \see StringInterner::getStats() for retrieving statistics.
 */
struct StringInternerStats {

  /**
   * \brief Total number of intern operations.
   */
  size_t internCount = 0;

  /**
   * \brief Total number of lookup operations.
   */
  size_t lookupCount = 0;

  /**
   * \brief Number of hash collisions encountered.
   */
  size_t collisionCount = 0;

  /**
   * \brief Total memory used for storing interned strings (in bytes).
   */
  size_t memoryUsedCount = 0;

  /**
   * \brief Number of unique interned strings.
   */
  size_t uniqueStringCount = 0;

  /**
   * \brief Average length of interned strings.
   */
  double averageLength = 0.0;
};

/**
 * \struct InternedStringHash "StringInterner.hpp" "ml/Basic/StringInterner.hpp"
 * \brief Hash function for \ref InternedString.
 */
struct InternedStringHash {

  /**
   * \brief Computes the hash value for an InternedString.
   * \param str The InternedString to hash.
   * \return The hash value.
   */
  size_t operator()(const InternedString &str) const {
    return str.getHashValue();
  }
};

/**
 * \class StringInterner StringInterner.hpp "ml/Basic/StringInterner.hpp"
 * \brief A string interner for deduplicating strings.
 * \details Manages a collection of unique strings, allowing efficient
 * storage and retrieval (O(1)) via \ref InternedString handles. Supports
 * optional arena allocation for improved memory locality through the \ref
 * ArenaAllocator.
 * \see InternedString for the interned string handle.
 * \note Thread-safe for concurrent access.
 * \warning \ref InternedString objects become invalid after \ref clear() is
 * called.
 */
class StringInterner {
public:
  /**
   * \brief Constructs a StringInterner without an ArenaAllocator.
   * \details Uses standard heap allocation for string storage.
   * \see StringInterner(ArenaAllocator &arena) for arena-based allocation.
   */
  StringInterner();

  /**
   * \brief Constructs a StringInterner with an ArenaAllocator.
   * \param arena The ArenaAllocator to use for string storage.
   * \see ArenaAllocator for memory management.
   */
  explicit StringInterner(ArenaAllocator &arena);

  ~StringInterner();

  /**
   * \brief Non-copyable StringInterner.
   * \details Copying a StringInterner is disallowed to prevent
   * accidental duplication of internal state and resource management issues.
   */
  StringInterner(const StringInterner &) = delete;

  /**
   * \brief Non-copyable assignment operator.
   * \see StringInterner(const StringInterner &)
   */
  StringInterner &operator=(const StringInterner &) = delete;

  /**
   * \brief Move constructor for StringInterner.
   * \details Transfers ownership of internal state and resources.
   */
  StringInterner(StringInterner &&other) noexcept;

  /**
   * \brief Move assignment operator for StringInterner.
   * \details Transfers ownership of internal state and resources.
   */
  StringInterner &operator=(StringInterner &&other) noexcept;

  /**
   * \brief Interns a string view.
   * \param str The string to intern.
   * \return The handle representing the interned string.
   */
  InternedString intern(std::string_view str);

  /**
   * \brief Interns a string.
   * \param str The string to intern.
   * \return The handle representing the interned string.
   * \see intern(std::string_view) for the main implementation.
   */
  InternedString intern(const std::string &str) {
    return intern(std::string_view(str));
  }

  /**
   * \brief Interns a C-style string.
   * \param str The C-style string to intern.
   * \return The handle representing the interned string.
   * \see intern(std::string_view) for the main implementation.
   */
  InternedString intern(const char *str) {
    return intern(std::string_view(str));
  }

  /**
   * \brief Interns a string with specified length.
   * \param str The C-style string to intern.
   * \param len The length of the string.
   * \return The handle representing the interned string.
   * \see intern(std::string_view) for the main implementation.
   */
  InternedString intern(const char *str, size_t len) {
    return intern(std::string_view(str, len));
  }

  /**
   * \brief Looks up an interned string.
   * \param str The string to look up.
   * \return The handle representing the interned string, or invalid if not
   * found.
   */
  InternedString lookup(std::string_view str) const;

  /**
   * \brief Checks if a string is interned.
   * \param str The string to check.
   * \return True if the string is interned, false otherwise.
   */
  bool contains(std::string_view str) const;

  /**
   * \brief Gets statistics about the StringInterner.
   * \return A \ref StringInternerStats object with current statistics.
   * \see StringInternerStats for details.
   */
  StringInternerStats getStats() const;

  /**
   * \brief Clears all interned strings.
   * \details Invalidates all existing \ref InternedString handles.
   * \note Resets internal state and statistics.
   */
  void clear();

  /**
   * \brief Gets the number of unique interned strings.
   * \return The count of unique interned strings.
   */
  size_t size() const;

  /**
   * \brief Checks if the interner is empty.
   * \return True if the interner has no interned strings, false otherwise.
   */
  bool empty() const;

  /**
   * \brief Prints statistics to the given output stream.
   * \param os The output stream to print to.
   */
  void printStats(std::ostream &os) const;

  /**
   * \brief Reserves space for a number of interned strings.
   * \param count The number of strings to reserve space for.
   */
  void reserve(size_t count);

  /**
   * \brief Gets the total memory usage of the interner.
   * \return The total memory used in bytes.
   */
  size_t getMemoryUsage() const;

  /**
   * \brief Checks if the interner is using an arena allocator.
   * \return True if using an arena allocator, false otherwise.
   */
  bool isUsingArena() const { return arenaAllocator != nullptr; }

  /**
   * \brief Gets the associated ArenaAllocator.
   * \return Pointer to the ArenaAllocator, or nullptr if not using one.
   */
  ArenaAllocator *getArena() const { return arenaAllocator; }

  /**
   * \class const_iterator
   * \brief Const iterator for iterating over interned strings.
   */
  class const_iterator {
  public:
    using value_type = InternedString;
    using difference_type = std::ptrdiff_t;
    using pointer = const InternedString *;
    using reference = const InternedString &;
    using iterator_category = std::forward_iterator_tag;

    const_iterator() = default;

    InternedString operator*() const;
    const_iterator &operator++();
    const_iterator operator++(int);

    bool operator==(const const_iterator &other) const;
    bool operator!=(const const_iterator &other) const;

  private:
    friend class StringInterner;
    explicit const_iterator(const StringInterner *interner, size_t index);

    const StringInterner *interner = nullptr;
    size_t index = 0;
  };

  const_iterator begin() const;
  const_iterator end() const;

private:
  /**
   * \struct StringStorage
   * \brief Internal storage for an interned string.
   */
  struct StringStorage {
    /**
     * \brief Pointer to the string data.
     */
    char *data;

    /**
     * \brief Size of the string data (in bytes).
     */
    size_t size;

    /**
     * \brief Indicates if the string uses arena allocation.
     */
    bool usesArena;

    StringStorage(std::string_view str, ArenaAllocator *arena = nullptr);
    ~StringStorage();

    StringStorage(const StringStorage &) = delete;
    StringStorage &operator=(const StringStorage &) = delete;
    StringStorage(StringStorage &&other) noexcept;
    StringStorage &operator=(StringStorage &&other) noexcept;

    const char *c_str() const { return data; }
  };

  /**
   * \brief Finds or creates the storage for a string.
   * \param str The string to find or create.
   * \return Pointer to the interned string data.
   */
  const char *findOrCreateString(std::string_view str);

  /**
   * \brief Pointer to the ArenaAllocator used for string storage.
   * \note nullptr if not using arena allocation.
   */
  ArenaAllocator *arenaAllocator;

  /**
   * \brief Mutex for thread-safe access.
   */
  mutable std::shared_mutex Mutex;

  /**
   * \brief Set of all unique StringStorage instances.
   * \note Manages the lifetime of interned strings.
   */
  std::unordered_set<std::unique_ptr<StringStorage>> Storage;

  /**
   * \brief Map from string views to interned string data pointers.
   * \note Enables fast lookup of interned strings.
   */
  std::unordered_map<std::string_view, const char *> LookupMap;

  /**
   * \brief Statistics about the \ref StringInterner.
   */
  mutable StringInternerStats stats;

  /**
   * \brief Hash function for StringStorage
   */
  struct StringStorageHash {
    size_t operator()(const std::unique_ptr<StringStorage> &storage) const;
  };

  /**
   * \brief Equality function for StringStorage
   */
  struct StringStorageEqual {
    bool operator()(const std::unique_ptr<StringStorage> &lhs,
                    const std::unique_ptr<StringStorage> &rhs) const;
  };
};

/**
 * \brief Stream output operator for InternedString.
 * \param os The output stream.
 * \param str The InternedString to output.
 * \return The output stream.
 */
inline std::ostream &operator<<(std::ostream &os, const InternedString &str) {
  return os << str.toStringView();
}

} // namespace ml

/**
 * \brief Specialization of std::hash for ml::InternedString.
 */
namespace std {
template <> struct hash<ml::InternedString> {
  size_t operator()(const ml::InternedString &str) const noexcept {
    return str.getHashValue();
  }
};
} // namespace std