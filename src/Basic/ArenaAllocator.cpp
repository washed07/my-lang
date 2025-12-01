#include "ml/Basic/ArenaAllocator.hpp"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace ml {

ArenaAllocator::ArenaAllocator(size_t chunkSize) : chunkSize(chunkSize) {
  // Ensure minimum chunk size
  if (this->chunkSize < 1024) {
    this->chunkSize = 1024;
  }

  // Allocate initial chunk
  allocateNewChunk();
}

ArenaAllocator::~ArenaAllocator() {
  // Chunks will be automatically destroyed
}

ArenaAllocator::ArenaAllocator(ArenaAllocator &&other) noexcept
    : Chunks(std::move(other.Chunks)), chunkSize(other.chunkSize),
      stats(other.stats) {
  // Reset the moved-from object
  other.stats = ArenaStats{};
}

ArenaAllocator &ArenaAllocator::operator=(ArenaAllocator &&other) noexcept {
  if (this != &other) {
    Chunks = std::move(other.Chunks);
    chunkSize = other.chunkSize;
    stats = other.stats;

    // Reset the moved-from object
    other.stats = ArenaStats{};
  }
  return *this;
}

void *ArenaAllocator::allocate(size_t size, size_t alignment) {
  if (size == 0) {
    return nullptr;
  }

  if (size > kMaxAllocationSize) {
    // For very large allocations, we could fall back to regular heap allocation
    // but for now, we'll just fail
    return nullptr;
  }

  // Ensure minimum alignment
  if (alignment < kDefaultAlignment) {
    alignment = kDefaultAlignment;
  }

  // Try to allocate from the current (last) chunk
  if (!Chunks.empty()) {
    size_t oldUsed = Chunks.back().used;
    void *ptr = Chunks.back().allocate(size, alignment);
    if (ptr) {
      size_t actualAllocated = Chunks.back().used - oldUsed;
      updateStats(size, actualAllocated);
      return ptr;
    }
  }

  // Need a new chunk
  size_t neededSize = size + alignment - 1; // Worst case alignment padding
  allocateNewChunk(std::max(neededSize, chunkSize));

  // Try again with the new chunk
  assert(!Chunks.empty());
  size_t oldUsed = Chunks.back().used;
  void *ptr = Chunks.back().allocate(size, alignment);

  if (ptr) {
    size_t actualAllocated = Chunks.back().used - oldUsed;
    updateStats(size, actualAllocated);
    return ptr;
  }

  // This should never happen if our math is correct
  return nullptr;
}

char *ArenaAllocator::allocateString(const char *str, size_t length) {
  if (!str) {
    return nullptr;
  }

  // Allocate space for string + null terminator
  char *result = static_cast<char *>(allocate(length + 1, 1));
  if (result) {
    std::memcpy(result, str, length);
    result[length] = '\0';
  }

  return result;
}

void ArenaAllocator::reset() {
  Chunks.clear();
  stats = ArenaStats{};

  // Allocate a fresh initial chunk
  allocateNewChunk();
}

void ArenaAllocator::clear() {
  // Reset all chunks to unused state
  for (auto &chunk : Chunks) {
    chunk.used = 0;
  }

  // Reset usage stats but keep allocation stats
  stats.currentUsage = 0;
  stats.allocationCount = 0;
}

ArenaStats ArenaAllocator::getStats() const {
  // Update current usage
  stats.currentUsage = 0;
  for (const auto &chunk : Chunks) {
    stats.currentUsage += chunk.used;
  }

  stats.peakUsage = std::max(stats.peakUsage, stats.currentUsage);
  return stats;
}

bool ArenaAllocator::contains(const void *ptr) const {
  const char *charPtr = static_cast<const char *>(ptr);

  for (const auto &chunk : Chunks) {
    const char *start = chunk.memory.get();
    const char *end = start + chunk.used;

    if (charPtr >= start && charPtr < end) {
      return true;
    }
  }

  return false;
}

size_t ArenaAllocator::getTotalAllocated() const {
  size_t total = 0;
  for (const auto &chunk : Chunks) {
    total += chunk.size;
  }
  return total;
}

size_t ArenaAllocator::getTotalUsed() const {
  size_t total = 0;
  for (const auto &chunk : Chunks) {
    total += chunk.used;
  }
  return total;
}

void ArenaAllocator::printStats(std::ostream &OS) const {
  auto stats = getStats();

  OS << "Arena Allocator Statistics:\n";
  OS << "  Total allocated: " << stats.allocatedCount << " bytes\n";
  OS << "  Total requested: " << stats.requestedCount << " bytes\n";
  OS << "  Current usage: " << stats.currentUsage << " bytes\n";
  OS << "  Peak usage: " << stats.peakUsage << " bytes\n";
  OS << "  Number of allocations: " << stats.allocationCount << "\n";
  OS << "  Number of chunks: " << stats.chunkCount << "\n";
  OS << "  Wasted bytes: " << stats.wastedByteCount << " bytes\n";
  OS << "  Fragmentation ratio: " << std::fixed << std::setprecision(2)
     << (stats.getFragmentationRatio() * 100.0) << "%\n";
  OS << "  Efficiency: " << std::fixed << std::setprecision(2)
     << (stats.getEfficiency() * 100.0) << "%\n";

  OS << "\nChunk details:\n";
  for (size_t i = 0; i < Chunks.size(); ++i) {
    const auto &chunk = Chunks[i];
    double utilization =
        chunk.size > 0 ? (static_cast<double>(chunk.used) / chunk.size) * 100.0
                       : 0.0;

    OS << "  Chunk " << i << ": " << chunk.used << "/" << chunk.size
       << " bytes (" << std::fixed << std::setprecision(1) << utilization
       << "% used)\n";
  }
}

void ArenaAllocator::allocateNewChunk(size_t minSize) {
  size_t newChunkSize = std::max(minSize, this->chunkSize);

  // Ensure chunk size is reasonable
  if (newChunkSize > 100 * 1024 * 1024) { // 100MB limit
    newChunkSize = 100 * 1024 * 1024;
  }

  Chunks.emplace_back(newChunkSize);

  // Update stats
  ++stats.chunkCount;
  stats.allocatedCount += newChunkSize;
}

void ArenaAllocator::updateStats(size_t requested, size_t allocated) const {
  ++stats.allocationCount;
  stats.requestedCount += requested;

  if (allocated > requested) {
    stats.wastedByteCount += (allocated - requested);
  }
}

} // namespace ml