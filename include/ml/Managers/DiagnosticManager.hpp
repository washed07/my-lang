#pragma once

#include "ml/Basic/SourceLocation.hpp"
#include "ml/Basic/StringInterner.hpp"
#include <functional>
#include <iosfwd>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ml {

class SourceManager;

/// Severity levels for diagnostics
enum class DiagnosticLevel { Note, Warning, Error, Fatal };

/// Categories of diagnostics
enum class DiagnosticKind {
  System,   // File I/O, memory, etc.
  Lexical,  // Tokenization issues
  Syntax,   // Parsing issues
  Semantic, // Type checking, name resolution
  Type,     // Type system issues
  Codegen,  // Code generation issues
  Link,     // Linking issues
  Runtime   // Runtime issues
};

/// Individual diagnostic definition
struct DiagnosticInfo {
  DiagnosticLevel Level;
  DiagnosticKind kind;
  std::string_view ShortMessage;
  std::string_view DetailedMessage;

  DiagnosticInfo(DiagnosticLevel level, DiagnosticKind kind,
                 std::string_view shortMsg, std::string_view detailedMsg)
      : Level(level), kind(kind), ShortMessage(shortMsg),
        DetailedMessage(detailedMsg) {}
};

/// Unique identifier for each diagnostic type
enum class DiagnosticID : uint32_t {
#define Diagnostic(ID, Level, Kind, Short, Detail) ID,
#include "ml/Diagnostics/Errors.inc"
#include "ml/Diagnostics/Notes.inc"
#include "ml/Diagnostics/Warnings.inc"
#undef Diagnostic
  NumDiagnostics
};

/// A single diagnostic instance with location and arguments
class Diagnostic {
public:
  Diagnostic(DiagnosticID id, SourceLocation loc = SourceLocation())
      : id(id), location(loc) {}

  /// Add string argument for diagnostic message formatting
  Diagnostic &addArg(std::string_view arg) {
    args.emplace_back(arg);
    return *this;
  }

  /// Add source range for highlighting
  Diagnostic &addRange(SourceRange range) {
    ranges.push_back(range);
    return *this;
  }

  /// Add fix-it hint
  Diagnostic &addFixIt(SourceRange range, std::string_view replacement) {
    fixits.emplace_back(range, replacement);
    return *this;
  }

  DiagnosticID getID() const { return id; }
  SourceLocation getLocation() const { return location; }
  const std::vector<std::string> &getArguments() const { return args; }
  const std::vector<SourceRange> &getRanges() const { return ranges; }

  struct FixItHint {
    SourceRange range;
    std::string replacement;

    FixItHint(SourceRange range, std::string_view replacement)
        : range(range), replacement(replacement) {}
  };

  const std::vector<FixItHint> &getFixItHints() const { return fixits; }

private:
  DiagnosticID id;
  SourceLocation location;
  std::vector<std::string> args;
  std::vector<SourceRange> ranges;
  std::vector<FixItHint> fixits;
};

/// Statistics about diagnostic reporting
struct DiagnosticStats {
  size_t noteCount = 0;
  size_t warningCount = 0;
  size_t errorCount = 0;
  size_t fatalCount = 0;
  size_t diagnosticCount = 0;

  bool hasErrors() const { return errorCount > 0 || fatalCount > 0; }
  bool hasWarnings() const { return warningCount > 0; }
};

/// Output format for diagnostics
enum class DiagnosticFormat {
  Text, // Human-readable text
  JSON, // Machine-readable JSON
  XML,  // Machine-readable XML
  SARIF // Static Analysis Results Interchange Format
};

/// Consumer interface for diagnostic output
class DiagnosticConsumer {
public:
  virtual ~DiagnosticConsumer() = default;

  /// Called before processing a batch of diagnostics
  virtual void beginSourceFile() {}

  /// Called to handle a single diagnostic
  virtual void handleDiagnostic(const Diagnostic &diag,
                                const DiagnosticInfo &info,
                                const SourceManager *srcMgr) = 0;

  /// Called after processing a batch of diagnostics
  virtual void endSourceFile() {}

  /// Called when diagnostics are finished
  virtual void finish() {}
};

/// Built-in text diagnostic consumer
class TextDiagnosticConsumer : public DiagnosticConsumer {
public:
  explicit TextDiagnosticConsumer(std::ostream &os, bool showColors = true)
      : os(os), showColor(showColors) {}

  void handleDiagnostic(const Diagnostic &diag, const DiagnosticInfo &info,
                        const SourceManager *srcMgr) override;

private:
  std::ostream &os;
  bool showColor;

  void printSourceLine(const SourceManager *srcMgr, SourceLocation loc,
                       const std::vector<SourceRange> &ranges);
  std::string formatMessage(std::string_view message,
                            const std::vector<std::string> &args);
};

/// Built-in JSON diagnostic consumer
class JSONDiagnosticConsumer : public DiagnosticConsumer {
public:
  explicit JSONDiagnosticConsumer(std::ostream &os) : os(os), firstDiag(true) {}

  void beginSourceFile() override;
  void handleDiagnostic(const Diagnostic &diag, const DiagnosticInfo &info,
                        const SourceManager *srcMgr) override;
  void endSourceFile() override;

private:
  std::ostream &os;
  bool firstDiag;
};

/// Main diagnostic manager class
class DiagnosticManager {
public:
  explicit DiagnosticManager(StringInterner &interner);
  ~DiagnosticManager();

  // Non-copyable but movable
  DiagnosticManager(const DiagnosticManager &) = delete;
  DiagnosticManager &operator=(const DiagnosticManager &) = delete;
  DiagnosticManager(DiagnosticManager &&) noexcept;
  DiagnosticManager &operator=(DiagnosticManager &&) noexcept;

  /// Set the source manager for location information
  void setSourceManager(const SourceManager *srcMgr) { this->srcMgr = srcMgr; }

  /// Add a diagnostic consumer
  void addConsumer(std::unique_ptr<DiagnosticConsumer> consumer);

  /// Remove all consumers
  void clearConsumers();

  /// Report a diagnostic
  void report(const Diagnostic &diag);

  /// Convenience methods for creating and reporting diagnostics
  void report(DiagnosticID id, SourceLocation loc = SourceLocation());
  void report(DiagnosticID id, SourceLocation loc, std::string_view arg1);
  void report(DiagnosticID id, SourceLocation loc, std::string_view arg1,
              std::string_view arg2);

  /// Diagnostic level filtering
  void setSuppressWarnings(bool suppress) { suppressWarnings = suppress; }
  void setSuppressNotes(bool suppress) { suppressNotes = suppress; }
  void setWarningsAsErrors(bool enable) { warningsAsErrors = enable; }
  void setMaxErrors(size_t max) { maxErrors = max; }

  bool getSuppressWarnings() const { return suppressWarnings; }
  bool getSuppressNotes() const { return suppressNotes; }
  bool getWarningsAsErrors() const { return warningsAsErrors; }
  size_t getMaxErrors() const { return maxErrors; }

  /// Get diagnostic statistics
  DiagnosticStats getStats() const;

  /// Check if there have been errors
  bool hasErrors() const;
  bool hasWarnings() const;
  bool hasFatalErrors() const;

  /// Reset statistics
  void reset();

  /// Get diagnostic information by id
  static const DiagnosticInfo &getDiagnosticInfo(DiagnosticID id);

  /// Check if we should continue compilation
  bool shouldContinue() const;

  /// Print statistics
  void printStats(std::ostream &os) const;

private:
  StringInterner &interner;
  const SourceManager *srcMgr = nullptr;

  std::vector<std::unique_ptr<DiagnosticConsumer>> consumers;

  // Configuration
  bool suppressWarnings = false;
  bool suppressNotes = false;
  bool warningsAsErrors = false;
  size_t maxErrors = 0; // 0 = unlimited

  // Statistics
  mutable DiagnosticStats stats;
  mutable std::mutex statsMutex;

  /// Static table of diagnostic information
  static const DiagnosticInfo sDiagnosticInfos[];

  /// Update statistics for a diagnostic
  void updateStats(DiagnosticLevel level) const;

  /// Check if diagnostic should be suppressed
  bool shouldSuppress(const DiagnosticInfo &info) const;
};

/// RAII helper for suppressing diagnostics in a scope
class DiagnosticSuppressor {
public:
  explicit DiagnosticSuppressor(DiagnosticManager &mgr)
      : mgr(mgr), oldSuppressWarnings(mgr.getSuppressWarnings()),
        oldSuppressNotes(mgr.getSuppressNotes()) {
    mgr.setSuppressWarnings(true);
    mgr.setSuppressNotes(true);
  }

  ~DiagnosticSuppressor() {
    mgr.setSuppressWarnings(oldSuppressWarnings);
    mgr.setSuppressNotes(oldSuppressNotes);
  }

private:
  DiagnosticManager &mgr;
  bool oldSuppressWarnings;
  bool oldSuppressNotes;
};

/// Convenience macros for reporting diagnostics
#define ML_DIAG_REPORT(mgr, id, loc) (mgr).report(DiagnosticID::id, loc)
#define ML_DIAG_REPORT_1(mgr, id, loc, arg1)                                   \
  (mgr).report(DiagnosticID::id, loc, arg1)
#define ML_DIAG_REPORT_2(mgr, id, loc, arg1, arg2)                             \
  (mgr).report(DiagnosticID::id, loc, arg1, arg2)

} // namespace ml