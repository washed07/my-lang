#include "ml/Basic/StringInterner.hpp"
#include "ml/Basic/ArenaAllocator.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <shared_mutex>

#ifdef _MSC_VER
#include <immintrin.h>
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <x86intrin.h>
#endif

// SIMD-optimized string comparison
static bool fast_string_equal(const char *a, const char *b, size_t len) {
#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
  // Use AVX2 for chunks of 32 bytes
  while (len >= 32) {
    __m256i chunk_a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a));
    __m256i chunk_b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(b));
    __m256i cmp = _mm256_cmpeq_epi8(chunk_a, chunk_b);

    if (_mm256_movemask_epi8(cmp) != static_cast<int>(0xFFFFFFFF)) {
      return false;
    }

    a += 32;
    b += 32;
    len -= 32;
  }
#elif defined(__SSE2__) || (defined(_MSC_VER) && defined(__SSE2__))
  // Use SSE2 for chunks of 16 bytes
  while (len >= 16) {
    __m128i chunk_a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(a));
    __m128i chunk_b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(b));
    __m128i cmp = _mm_cmpeq_epi8(chunk_a, chunk_b);

    if (_mm_movemask_epi8(cmp) != 0xFFFF) {
      return false;
    }

    a += 16;
    b += 16;
    len -= 16;
  }
#endif

  // Handle remaining bytes
  return std::memcmp(a, b, len) == 0;
}

namespace ml {

// StringStorage implementation with arena allocation support
StringInterner::StringStorage::StringStorage(std::string_view str,
                                             ArenaAllocator *arena)
    : size(str.size()), usesArena(arena != nullptr) {
  // Allocate memory either from arena or heap
  if (arena) {
    // Use arena allocation for better memory locality
    data = arena->allocateString(str.data(), str.size());
    if (!data) {
      throw std::bad_alloc();
    }
  } else {
    // Use aligned heap allocation for SIMD operations
    size_t aligned_size = (size + 16) & ~15;
    auto heap_data = std::make_unique<char[]>(aligned_size);

    // Use fast copy for larger strings
    if (size >= 32) {
      std::memcpy(heap_data.get(), str.data(), size);
    } else {
      // Manual copy for small strings to avoid function call overhead
      const char *src = str.data();
      char *dst = heap_data.get();
      for (size_t i = 0; i < size; ++i) {
        dst[i] = src[i];
      }
    }
    heap_data[size] = '\0'; // Null terminate

    // Transfer ownership to raw pointer
    data = heap_data.release();
  }
}

StringInterner::StringStorage::~StringStorage() {
  // Only delete if not using arena (arena manages its own memory)
  if (!usesArena && data) {
    delete[] data;
  }
}

StringInterner::StringStorage::StringStorage(StringStorage &&other) noexcept
    : data(other.data), size(other.size), usesArena(other.usesArena) {
  other.data = nullptr;
  other.size = 0;
}

StringInterner::StringStorage &
StringInterner::StringStorage::operator=(StringStorage &&other) noexcept {
  if (this != &other) {
    // Clean up current data
    if (!usesArena && data) {
      delete[] data;
    }

    // Transfer ownership
    data = other.data;
    size = other.size;
    usesArena = other.usesArena;

    // Reset other
    other.data = nullptr;
    other.size = 0;
  }
  return *this;
}

// StringInterner implementation
StringInterner::StringInterner() : arenaAllocator(nullptr) {
  // Reserve some initial space to avoid early rehashing
  Storage.reserve(1000);
  LookupMap.reserve(1000);
}

StringInterner::StringInterner(ArenaAllocator &arena) : arenaAllocator(&arena) {
  // Reserve some initial space to avoid early rehashing
  Storage.reserve(1000);
  LookupMap.reserve(1000);
}

StringInterner::~StringInterner() = default;

StringInterner::StringInterner(StringInterner &&other) noexcept
    : arenaAllocator(other.arenaAllocator), Storage(std::move(other.Storage)),
      LookupMap(std::move(other.LookupMap)), stats(other.stats) {
  other.arenaAllocator = nullptr;
}

StringInterner &StringInterner::operator=(StringInterner &&other) noexcept {
  if (this != &other) {
    std::unique_lock<std::shared_mutex> lock(Mutex);
    std::unique_lock<std::shared_mutex> otherLock(other.Mutex);

    Storage = std::move(other.Storage);
    LookupMap = std::move(other.LookupMap);
    stats = other.stats;
    arenaAllocator = other.arenaAllocator;

    other.arenaAllocator = nullptr;
  }
  return *this;
}

InternedString StringInterner::intern(std::string_view str) {
  ++stats.lookupCount;

  // Early exit for empty strings
  if (str.empty()) {
    static const char empty_str[] = "";
    return InternedString(empty_str);
  }

  // Early optimization: compute string length once (for potential future use)
  // size_t str_length = str.length();

  // Fast path: check if already interned (shared lock)
  {
    std::shared_lock<std::shared_mutex> lock(Mutex);
    auto it = LookupMap.find(str);
    if (it != LookupMap.end()) {
      return InternedString(it->second);
    }
  }

  // Slow path: need to intern the string (exclusive lock)
  std::unique_lock<std::shared_mutex> lock(Mutex);

  // Double-check that another thread didn't intern it while we were waiting
  auto it = LookupMap.find(str);
  if (it != LookupMap.end()) {
    return InternedString(it->second);
  }

  // Create new storage for the string (using arena if available)
  auto storage = std::make_unique<StringStorage>(str, arenaAllocator);
  const char *ptr = storage->c_str();

  // Reserve capacity if needed to avoid rehashing
  if (Storage.size() >= Storage.bucket_count() * 0.75) {
    Storage.reserve(Storage.size() * 2);
    LookupMap.reserve(LookupMap.size() * 2);
  }

  // Add to storage set
  auto [storageIt, storageInserted] = Storage.insert(std::move(storage));
  if (!storageInserted) {
    // This should never happen with our current implementation
    ++stats.collisionCount;
    return InternedString((*storageIt)->c_str());
  }

  // Add to lookup map using the stored string as the key
  std::string_view storedView(ptr, str.size());
  LookupMap[storedView] = ptr;

  // Update statistics efficiently
  ++stats.internCount;
  ++stats.uniqueStringCount;
  size_t string_memory = str.size() + 1; // +1 for null terminator
  stats.memoryUsedCount += string_memory;

  // Update average length using incremental formula
  stats.averageLength =
      (stats.averageLength * (stats.uniqueStringCount - 1) + str.size()) /
      stats.uniqueStringCount;

  return InternedString(ptr);
}

InternedString StringInterner::lookup(std::string_view str) const {
  std::shared_lock<std::shared_mutex> lock(Mutex);

  auto it = LookupMap.find(str);
  if (it != LookupMap.end()) {
    return InternedString(it->second);
  }

  return InternedString(); // Invalid/empty string
}

bool StringInterner::contains(std::string_view str) const {
  std::shared_lock<std::shared_mutex> lock(Mutex);
  return LookupMap.count(str) > 0;
}

StringInternerStats StringInterner::getStats() const {
  std::shared_lock<std::shared_mutex> lock(Mutex);
  return stats;
}

void StringInterner::clear() {
  std::unique_lock<std::shared_mutex> lock(Mutex);

  Storage.clear();
  LookupMap.clear();

  // Reset statistics
  stats = StringInternerStats{};
}

size_t StringInterner::size() const {
  std::shared_lock<std::shared_mutex> lock(Mutex);
  return Storage.size();
}

bool StringInterner::empty() const {
  std::shared_lock<std::shared_mutex> lock(Mutex);
  return Storage.empty();
}

void StringInterner::printStats(std::ostream &os) const {
  auto stats = getStats();

  os << "StringInterner Statistics:\n";
  os << "  Unique strings: " << stats.uniqueStringCount << "\n";
  os << "  Total lookups: " << stats.lookupCount << "\n";
  os << "  Strings interned: " << stats.internCount << "\n";
  os << "  Hash collisions: " << stats.collisionCount << "\n";
  os << "  Memory used: " << stats.memoryUsedCount << " bytes\n";
  os << "  Average string length: " << stats.averageLength << " chars\n";

  if (stats.lookupCount > 0) {
    double hitRate =
        static_cast<double>(stats.lookupCount - stats.internCount) /
        stats.lookupCount;
    os << "  Cache hit rate: " << (hitRate * 100.0) << "%\n";
  }
}

void StringInterner::reserve(size_t count) {
  std::unique_lock<std::shared_mutex> lock(Mutex);

  Storage.reserve(count);
  LookupMap.reserve(count);
}

size_t StringInterner::getMemoryUsage() const {
  std::shared_lock<std::shared_mutex> lock(Mutex);

  size_t totalMemory = stats.memoryUsedCount; // String data
  totalMemory += Storage.size() *
                 sizeof(std::unique_ptr<StringStorage>); // Storage overhead
  totalMemory += LookupMap.size() * (sizeof(std::string_view) +
                                     sizeof(const char *)); // Map overhead

  return totalMemory;
}

// Iterator implementation
StringInterner::const_iterator::const_iterator(const StringInterner *interner,
                                               size_t index)
    : interner(interner), index(index) {}

InternedString StringInterner::const_iterator::operator*() const {
  if (!interner || index >= interner->Storage.size()) {
    return InternedString();
  }

  auto it = interner->Storage.begin();
  std::advance(it, index);
  return InternedString((*it)->c_str());
}

StringInterner::const_iterator &StringInterner::const_iterator::operator++() {
  ++index;
  return *this;
}

StringInterner::const_iterator StringInterner::const_iterator::operator++(int) {
  const_iterator tmp = *this;
  ++index;
  return tmp;
}

bool StringInterner::const_iterator::operator==(
    const const_iterator &other) const {
  return interner == other.interner && index == other.index;
}

bool StringInterner::const_iterator::operator!=(
    const const_iterator &other) const {
  return !(*this == other);
}

StringInterner::const_iterator StringInterner::begin() const {
  std::shared_lock<std::shared_mutex> lock(Mutex);
  return const_iterator(this, 0);
}

StringInterner::const_iterator StringInterner::end() const {
  std::shared_lock<std::shared_mutex> lock(Mutex);
  return const_iterator(this, Storage.size());
}

const char *StringInterner::findOrCreateString(std::string_view str) {
  return intern(str).toCStr();
}

// Hash and equality functions for StringStorage
size_t StringInterner::StringStorageHash::operator()(
    const std::unique_ptr<StringStorage> &storage) const {
  if (!storage || !storage->data)
    return 0;

  // Use std::hash for string_view
  return std::hash<std::string_view>{}(
      std::string_view(storage->c_str(), storage->size));
}

bool StringInterner::StringStorageEqual::operator()(
    const std::unique_ptr<StringStorage> &lhs,
    const std::unique_ptr<StringStorage> &rhs) const {
  if (!lhs || !rhs || !lhs->data || !rhs->data)
    return lhs == rhs;
  if (lhs->size != rhs->size)
    return false;

  // Use SIMD-optimized comparison for better performance
  return fast_string_equal(lhs->data, rhs->data, lhs->size);
}

} // namespace ml