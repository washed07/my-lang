#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iosfwd>
#include <list>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace ml {

/**
 * \struct ArenaStats ArenaAllocator.hpp "ml/Basic/ArenaAllocator.hpp"
 * \brief A container for statistics about \ref ArenaAllocator usage.
 * \details Holds various statistics about the memory usage
 * of an \ref ArenaAllocator instance, including:
 * \li Total allocated bytes,
 * \li Requested bytes,
 * \li Allocation count,
 * \li Chunk count,
 * \li Peak usage,
 * \li Current usage,
 * \li Wasted bytes due to fragmentation or alignment.
 * It also provides methods to compute fragmentation ratio and efficiency.
 * \see ArenaAllocator for usage context.
 */
struct ArenaStats {

  /**
   * \brief The total bytes allocated from the system.
   * \details Tracks the total number of bytes allocated by the
   * arena allocator from the underlying system or heap.
   */
  size_t allocatedCount = 0;

  /**
   * \brief The total bytes requested by the user.
   * \details Tracks the total number of bytes requested by the
   * user through allocation calls.
   */
  size_t requestedCount = 0;

  /**
   * \brief The number of allocation calls made.
   * \details Tracks how many times memory allocation was requested
   * from the arena allocator.
   */
  size_t allocationCount = 0;

  /**
   * \brief The number of memory chunks.
   * \details Tracks how many memory chunks have been allocated
   * by the arena allocator.
   */
  size_t chunkCount = 0;

  /**
   * \brief The peak memory usage.
   * \details Tracks the highest amount of memory used at any
   * point in time.
   */
  size_t peakUsage = 0;

  /**
   * \brief The current memory usage.
   */
  size_t currentUsage = 0;

  /**
   * \brief The bytes lost to alignment or fragmentation.
   * \details Tracks the number of bytes wasted due to alignment
   * requirements or fragmentation within the arena allocator.
   */
  size_t wastedByteCount = 0;

  /**
   * \brief Gets the fragmentation ratio.
   * \return Fragmentation ratio as a double in [0.0, 1.0]
   */
  double getFragmentationRatio() const {
    return requestedCount > 0
               ? static_cast<double>(wastedByteCount) / requestedCount
               : 0.0;
  }

  /**
   * \brief Gets the allocation efficiency.
   * \return Efficiency ratio as a double in [0.0, 1.0]
   */
  double getEfficiency() const {
    return allocatedCount > 0
               ? static_cast<double>(requestedCount) / allocatedCount
               : 0.0;
  }
};

/**
 * \struct ArenaChunk ArenaAllocator.hpp "ml/Basic/ArenaAllocator.hpp"
 * \brief A chunk of memory managed by \ref ArenaAllocator.
 * \details Encapsulates a contiguous block of memory managed by
 * the \ref ArenaAllocator. It tracks the size of the chunk, the amount of
 * memory used, and provides methods for allocation within the chunk.
 * \see ArenaAllocator for context.
 */
struct ArenaChunk {

  /**
   * \brief The memory block for this chunk.
   * \details A unique pointer managing the memory allocated for this chunk.
   */
  std::unique_ptr<char[]> memory;

  /**
   * \brief The total size of the chunk in bytes.
   */
  size_t size;

  /**
   * \brief The amount of memory used in bytes.
   */
  size_t used;

  ArenaChunk(size_t size)
      : memory(std::make_unique<char[]>(size)), size(size), used(0) {}

  ArenaChunk(const ArenaChunk &) = delete;
  ArenaChunk &operator=(const ArenaChunk &) = delete;

  ArenaChunk(ArenaChunk &&) = default;
  ArenaChunk &operator=(ArenaChunk &&) = default;

  /**
   * \brief Gets a pointer to the unused memory in this chunk.
   * \return A pointer to the start of unused memory
   */
  char *getUnused() const { return memory.get() + used; }

  /**
   * \brief Gets the number of remaining bytes in this chunk.
   * \return The number of remaining bytes
   */
  size_t getRemaining() const { return size - used; }

  /**
   * \brief Checks if the chunk can fit a requested size.
   * \param size The size to check
   * \return True if the chunk can accommodate the requested size
   */
  bool canFit(size_t size) const { return getRemaining() >= size; }

  /**
   * \brief Allocates memory within this chunk.
   * \param size The size of memory to allocate
   * \param alignment The required alignment
   * \return A pointer to the allocated memory or nullptr if allocation fails.
   */
  void *allocate(size_t size, size_t alignment) {
    char *ptr = getUnused();

    // Align the pointer
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    char *alignedPtr = reinterpret_cast<char *>(aligned);

    size_t totalSize = (alignedPtr - ptr) + size;

    if (totalSize > getRemaining()) {
      return nullptr; // Cannot fit
    }

    used += totalSize;
    return alignedPtr;
  }
};

/**
 * \class ArenaAllocator ArenaAllocator.hpp "ml/Basic/ArenaAllocator.hpp"
 * \brief An allocator using arena allocation strategy.
 * \details Manages memory in large chunks to provide fast allocation and
 * deallocation of small to medium-sized objects, minimizing fragmentation and
 * overhead. It is particularly well-suited for compiler use cases where many
 * temporary objects are created.
 * \see ArenaStats for tracking allocation statistics.
 * \see ArenaChunk for individual memory chunk management.
 */
class ArenaAllocator {
public:
  /**
   * \brief The default chunk size for allocations. 1MB
   */
  static constexpr size_t kDefaultChunkSize = 1024 * 1024;

  /**
   * \brief The default alignment for allocations. 16 bytes
   */
  static constexpr size_t kDefaultAlignment = alignof(std::max_align_t);

  /**
   * \brief The maximum allocation size supported by the arena. 512KB
   */
  static constexpr size_t kMaxAllocationSize = 512 * 1024;

  explicit ArenaAllocator(size_t chunkSize = kDefaultChunkSize);
  ~ArenaAllocator();

  ArenaAllocator(const ArenaAllocator &) = delete;
  ArenaAllocator &operator=(const ArenaAllocator &) = delete;
  ArenaAllocator(ArenaAllocator &&) noexcept;
  ArenaAllocator &operator=(ArenaAllocator &&) noexcept;

  /**
   * \brief Allocates memory with default alignment.
   * \param size The size of memory to allocate
   * \return A pointer to the allocated memory or nullptr if allocation fails.
   */
  void *allocate(size_t size) { return allocate(size, kDefaultAlignment); }

  /**
   * \brief Allocates memory with specified alignment.
   * \param size The size of memory to allocate
   * \param alignment The required alignment
   * \return A pointer to the allocated memory or nullptr if allocation fails.
   */
  void *allocate(size_t size, size_t alignment);

  /**
   * \brief Allocates and constructs an object of type T.
   * \tparam T The type of object to allocate
   * \tparam Args The constructor argument types
   * \param args The constructor arguments
   * \return A pointer to the constructed object.
   */
  template <typename T, typename... Args> T *allocate(Args &&...args) {
    static_assert(sizeof(T) <= kMaxAllocationSize,
                  "Object too large for arena allocation");

    void *ptr = allocate(sizeof(T), alignof(T));
    if (!ptr) {
      throw std::bad_alloc();
    }

    return new (ptr) T(std::forward<Args>(args)...);
  }

  /**
   * \brief Allocates an array of objects.
   * \tparam T The type of objects to allocate
   * \param count The number of objects to allocate
   * \return A pointer to the allocated array or nullptr if allocation fails.
   */
  template <typename T> T *allocateArray(size_t count) {
    static_assert(std::is_trivially_destructible_v<T>,
                  "Arena arrays only support trivially destructible types");

    size_t totalSize = sizeof(T) * count;
    if (totalSize > kMaxAllocationSize) {
      throw std::bad_alloc();
    }

    void *ptr = allocate(totalSize, alignof(T));
    if (!ptr) {
      throw std::bad_alloc();
    }

    return static_cast<T *>(ptr);
  }

  /**
   * \brief Allocates a string in the arena.
   * \param str The string data
   * \param length The length of the string
   * \return A pointer to the allocated string.
   */
  char *allocateString(const char *str, size_t length);
  char *allocateString(const char *str) {
    return allocateString(str, strlen(str));
  }

  /**
   * \brief Resets the arena, freeing all allocated memory.
   * \details This invalidates all previously allocated memory.
   */
  void reset();

  /**
   * \brief Clears the arena, resetting all chunks.
   * \details This does not free memory but makes it available for reuse.
   * \see reset() for full deallocation.
   */
  void clear();

  /**
   * \brief Gets the current allocation statistics.
   * \return The current \ref ArenaStats.
   */
  ArenaStats getStats() const;

  /**
   * \brief Checks if a pointer belongs to this arena.
   * \param ptr The pointer to check
   * \return True if the pointer was allocated by this arena
   */
  bool contains(const void *ptr) const;

  /**
   * \brief Get total allocated memory
   * \return Total allocated memory in bytes
   */
  size_t getTotalAllocated() const;

  /**
   * \brief Get total used memory
   * \return Total used memory in bytes
   */
  size_t getTotalUsed() const;

  /**
   * \brief Print statistics
   * \param OS The output stream to print to
   */
  void printStats(std::ostream &OS) const;

  /**
   * \brief Sets the chunk size for future allocations.
   * \param size The new chunk size in bytes
   */
  void setChunkSize(size_t size) { chunkSize = size; }

  /**
   * \brief Gets the current chunk size.
   * \return The chunk size in bytes
   */
  size_t getChunkSize() const { return chunkSize; }

private:
  /**
   * \brief The list of memory chunks managed by the arena.
   */
  std::vector<ArenaChunk> Chunks;

  /**
   * \brief The preferred chunk size for allocations.
   */
  size_t chunkSize;

  /**
   * \brief Statistics about arena usage.
   */
  mutable ArenaStats stats;

  /**
   * \brief Allocates a new memory chunk.
   * \param minSize The minimum size required for the chunk
   */
  void allocateNewChunk(size_t minSize = 0);

  /**
   * \brief Updates allocation statistics.
   * \param requested The number of bytes requested
   * \param allocated The number of bytes actually allocated
   */
  void updateStats(size_t requested, size_t allocated) const;
};

/**
 * \class ArenaScope ArenaAllocator.hpp "ml/Basic/ArenaAllocator.hpp"
 * \brief A scope guard for arena allocation.
 * \details Saves the state of an \ref ArenaAllocator upon construction
 * and can restore it upon destruction. This is useful for managing
 * temporary allocations within a specific scope.
 * \see ArenaAllocator for context.
 */
class ArenaScope {
public:
  explicit ArenaScope(ArenaAllocator &arena) : arena(arena) {
    savedStats = arena.getStats();
  }

  ~ArenaScope() {
    // TODO: Optionally reset arena to saved state
  }

private:
  /**
   * \brief The arena allocator being scoped.
   */
  ArenaAllocator &arena;

  /**
   * \brief The saved statistics at the start of the scope.
   */
  ArenaStats savedStats;
};

/**
 * \class ArenaSTLAllocator ArenaAllocator.hpp "ml/Basic/ArenaAllocator.hpp"
 * \brief STL-compatible allocator using \ref ArenaAllocator.
 * \tparam T The type of objects to allocate.
 * \details This class provides an STL-compatible allocator that uses
 * \ref ArenaAllocator for memory management. It can be used with standard
 * containers like \c std::vector, \c std::list, and \c std::deque to
 * allocate memory from an arena, improving performance and memory locality.
 * \see ArenaAllocator for the underlying allocation strategy.
 */
template <typename T> class ArenaSTLAllocator {
public:
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  /**
   * \brief Rebind allocator to another type U.
   * \tparam U The new type to bind to.
   */
  template <typename U> struct rebind {
    using other = ArenaSTLAllocator<U>;
  };

  explicit ArenaSTLAllocator(ArenaAllocator &arena) : arena(&arena) {}

  template <typename U>
  ArenaSTLAllocator(const ArenaSTLAllocator<U> &other) : arena(other.arena) {}

  /**
   * \brief Allocates memory for n objects of type T.
   * \param n The number of objects to allocate.
   * \return A pointer to the allocated memory.
   */
  T *allocate(size_type n) {
    if (n > ArenaAllocator::kMaxAllocationSize / sizeof(T)) {
      throw std::bad_alloc();
    }

    void *ptr = arena->allocate(n * sizeof(T), alignof(T));
    if (!ptr) {
      throw std::bad_alloc();
    }

    return static_cast<T *>(ptr);
  }

  /**
   * \brief Deallocates memory for n objects of type T.
   * \param ptr Pointer to the memory to deallocate.
   * \param n The number of objects to deallocate.
   */
  void deallocate(T *ptr, size_type n) {
    // Arena allocator doesn't support individual deallocation
    (void)ptr;
    (void)n;
  }

  template <typename U>
  bool operator==(const ArenaSTLAllocator<U> &other) const {
    return arena == other.arena;
  }

  template <typename U>
  bool operator!=(const ArenaSTLAllocator<U> &other) const {
    return !(*this == other);
  }

private:
  template <typename U> friend class ArenaSTLAllocator;
  ArenaAllocator *arena;
};

/// Convenience aliases for STL containers with arena allocation

/**
 * \brief STL vector using arena allocation.
 * \tparam T The type of objects in the vector.
 */
template <typename T> using ArenaVector = std::vector<T, ArenaSTLAllocator<T>>;

/**
 * \brief STL deque using arena allocation.
 * \tparam T The type of objects in the deque.
 */
template <typename T> using ArenaDeque = std::deque<T, ArenaSTLAllocator<T>>;

/**
 * \brief STL list using arena allocation.
 * \tparam T The type of objects in the list.
 */
template <typename T> using ArenaList = std::list<T, ArenaSTLAllocator<T>>;

} // namespace ml