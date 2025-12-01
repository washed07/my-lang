#pragma once

#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ml {

class ArenaAllocator;

/**
 * \brief An interned string handle.
 * \details Represents a unique interned string stored in the
 * \ref StringInterner. Provides fast comparison via pointer equality
 * and access to the underlying string data.
 * \see StringInterner for managing interned strings.
 */
class InternedString {
public:
  InternedString() : ptr(nullptr) {}

  /**
   * \brief Get the C-style string pointer.
   * \return A pointer to the null-terminated string.
   */
  const char *c_str() const { return ptr ? ptr : ""; }

  /**
   * \brief Get the string as a string view.
   * \return A string view representing the interned string.
   */
  std::string_view str() const {
    return ptr ? std::string_view(ptr) : std::string_view();
  }

  /**
   * \brief Get the length of the interned string.
   * \return The length of the string in characters.
   * \deprecated Use \ref length() instead.
   * \see length()
   */
  size_t size() const { return str().size(); }

  /**
   * \brief Get the length of the interned string.
   * \return The length of the string in characters.
   */
  size_t length() const { return size(); }

  /**
   * \brief Check if the interned string is empty.
   * \return True if the string is empty, false otherwise.
   */
  bool empty() const { return ptr == nullptr || *ptr == '\0'; }

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
   * \brief Compare with a string view for equality.
   * \param other The string view to compare with.
   * \see operator==(const InternedString &) const for pointer comparison.
   * \note Use pointer comparison for best performance when possible.
   */
  bool equals(std::string_view other) const { return str() == other; }

  /**
   * \brief Check if the interned string is valid.
   * \return True if valid, false otherwise.
   */
  bool isValid() const { return ptr != nullptr; }

  /**
   * \brief Get the hash value of the interned string.
   * \return The hash value based on the string pointer.
   */
  size_t getHash() const { return std::hash<const char *>{}(ptr); }

  /**
   * \brief Convert to string.
   * \return A string copy of the interned string.
   */
  std::string toString() const { return std::string(str()); }

  operator std::string_view() const { return str(); }

private:
  friend class StringInterner;
  explicit InternedString(const char *ptr) : ptr(ptr) {}

  /**
   * \brief The pointer to the interned string data.
   */
  const char *ptr;
};

/// Statistics about string interner operations
struct StringInternerStats {
  size_t internCount = 0;
  size_t lookupCount = 0;
  size_t collisionCount = 0;
  size_t memoryUsedCount = 0;
  size_t uniqueStringCount = 0;
  double averageLength = 0.0;
};

/// Hash function for InternedString
struct InternedStringHash {
  size_t operator()(const InternedString &str) const { return str.getHash(); }
};

/// A high-performance string interner that stores unique strings and provides
/// fast comparison via pointer equality. This is essential for compiler
/// performance when dealing with identifiers, keywords, and string literals.
///
/// Key features:
/// - Thread-safe operations
/// - Fast O(1) equality comparison via pointer comparison
/// - Memory-efficient storage with automatic deduplication
/// - Support for string_view to avoid unnecessary allocations
/// - Statistics and debugging support
class StringInterner {
public:
  StringInterner();

  /// Create a StringInterner that uses an external arena allocator
  /// for improved memory locality and reduced heap fragmentation
  explicit StringInterner(ArenaAllocator &arena);

  ~StringInterner();

  // Non-copyable but movable
  StringInterner(const StringInterner &) = delete;
  StringInterner &operator=(const StringInterner &) = delete;
  StringInterner(StringInterner &&other) noexcept;
  StringInterner &operator=(StringInterner &&other) noexcept;

  /// Intern a string from various sources
  InternedString intern(std::string_view str);
  InternedString intern(const std::string &str) {
    return intern(std::string_view(str));
  }
  InternedString intern(const char *str) {
    return intern(std::string_view(str));
  }
  InternedString intern(const char *str, size_t len) {
    return intern(std::string_view(str, len));
  }

  /// Lookup a string without interning (returns invalid InternedString if not
  /// found)
  InternedString lookup(std::string_view str) const;

  /// Check if a string is already interned
  bool contains(std::string_view str) const;

  /// Get statistics about the interner
  StringInternerStats getStats() const;

  /// Clear all interned strings (invalidates all InternedString objects)
  void clear();

  /// Get the number of unique strings
  size_t size() const;

  /// Check if the interner is empty
  bool empty() const;

  /// Print statistics to the given stream
  void printStats(std::ostream &os) const;

  /// Reserve space for approximately this many strings (optimization)
  void reserve(size_t count);

  /// Get memory usage in bytes
  size_t getMemoryUsage() const;

  /// Check if using arena allocation
  bool isUsingArena() const { return arenaAllocator != nullptr; }

  /// Get the arena allocator (if any)
  ArenaAllocator *getArena() const { return arenaAllocator; }

  /// Iterator support for debugging/inspection
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
  /// Internal storage for string data
  struct StringStorage {
    char *data; // Changed to raw pointer for arena compatibility
    size_t size;
    bool usesArena; // Track whether this uses arena allocation

    StringStorage(std::string_view str, ArenaAllocator *arena = nullptr);
    ~StringStorage();

    // Non-copyable but movable
    StringStorage(const StringStorage &) = delete;
    StringStorage &operator=(const StringStorage &) = delete;
    StringStorage(StringStorage &&other) noexcept;
    StringStorage &operator=(StringStorage &&other) noexcept;

    const char *c_str() const { return data; }
  };

  /// Find or create storage for a string
  const char *findOrCreateString(std::string_view str);

  /// Arena allocator for improved memory locality (optional)
  ArenaAllocator *arenaAllocator;

  /// Thread safety
  mutable std::shared_mutex Mutex;

  /// String storage - we use a set for automatic deduplication
  /// The strings are stored as unique_ptr<StringStorage> to ensure
  /// pointer stability even when the container is rehashed
  std::unordered_set<std::unique_ptr<StringStorage>> Storage;

  /// Fast lookup map from string content to pointer
  std::unordered_map<std::string_view, const char *> LookupMap;

  /// Statistics
  mutable StringInternerStats stats;

  /// Hash function for StringStorage
  struct StringStorageHash {
    size_t operator()(const std::unique_ptr<StringStorage> &storage) const;
  };

  /// Equality function for StringStorage
  struct StringStorageEqual {
    bool operator()(const std::unique_ptr<StringStorage> &lhs,
                    const std::unique_ptr<StringStorage> &rhs) const;
  };
};

/// Stream output operator for InternedString
inline std::ostream &operator<<(std::ostream &os, const InternedString &str) {
  return os << str.str();
}

} // namespace ml

/// Specialize std::hash for InternedString to enable use in standard containers
namespace std {
template <> struct hash<ml::InternedString> {
  size_t operator()(const ml::InternedString &str) const noexcept {
    return str.getHash();
  }
};
} // namespace std