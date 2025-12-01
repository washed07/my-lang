#include "ml/Managers/DiagnosticManager.hpp"
#include "ml/Managers/SourceManager.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

namespace ml {

// Static diagnostic information table
const DiagnosticInfo DiagnosticManager::sDiagnosticInfos[] = {
#define Diagnostic(ID, Level, Kind, Short, Detail)                             \
  DiagnosticInfo(Level, Kind, Short, Detail),
#include "ml/Diagnostics/Errors.inc"
#include "ml/Diagnostics/Notes.inc"
#include "ml/Diagnostics/Warnings.inc"
#undef Diagnostic
};

// DiagnosticManager implementation
DiagnosticManager::DiagnosticManager(StringInterner &interner)
    : interner(interner) {}

DiagnosticManager::~DiagnosticManager() = default;

DiagnosticManager::DiagnosticManager(DiagnosticManager &&other) noexcept
    : interner(other.interner), srcMgr(other.srcMgr),
      consumers(std::move(other.consumers)),
      suppressWarnings(other.suppressWarnings),
      suppressNotes(other.suppressNotes),
      warningsAsErrors(other.warningsAsErrors), maxErrors(other.maxErrors),
      stats(other.stats) {}

void DiagnosticManager::addConsumer(
    std::unique_ptr<DiagnosticConsumer> consumer) {
  consumers.push_back(std::move(consumer));
}

void DiagnosticManager::clearConsumers() { consumers.clear(); }

void DiagnosticManager::report(const Diagnostic &diag) {
  const DiagnosticInfo &info = getDiagnosticInfo(diag.getID());

  // Check if we should suppress this diagnostic
  if (shouldSuppress(info)) {
    return;
  }

  // Treat warnings as errors if configured
  DiagnosticLevel effectiveLevel = info.Level;
  if (warningsAsErrors && info.Level == DiagnosticLevel::Warning) {
    effectiveLevel = DiagnosticLevel::Error;
  }

  // Update statistics
  updateStats(effectiveLevel);

  // Check if we've hit the error limit
  if (maxErrors > 0 && stats.errorCount >= maxErrors) {
    return;
  }

  // Create a modified info with the effective level
  DiagnosticInfo effectiveInfo = info;
  effectiveInfo.Level = effectiveLevel;

  // Report to all consumers
  for (auto &consumer : consumers) {
    consumer->handleDiagnostic(diag, effectiveInfo, srcMgr);
  }
}

void DiagnosticManager::report(DiagnosticID id, SourceLocation loc) {
  Diagnostic diag(id, loc);
  report(diag);
}

void DiagnosticManager::report(DiagnosticID id, SourceLocation loc,
                               std::string_view arg1) {
  Diagnostic diag(id, loc);
  diag.addArg(arg1);
  report(diag);
}

void DiagnosticManager::report(DiagnosticID id, SourceLocation loc,
                               std::string_view arg1, std::string_view arg2) {
  Diagnostic diag(id, loc);
  diag.addArg(arg1).addArg(arg2);
  report(diag);
}

DiagnosticStats DiagnosticManager::getStats() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  return stats;
}

bool DiagnosticManager::hasErrors() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  return stats.hasErrors();
}

bool DiagnosticManager::hasWarnings() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  return stats.hasWarnings();
}

bool DiagnosticManager::hasFatalErrors() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  return stats.fatalCount > 0;
}

void DiagnosticManager::reset() {
  std::lock_guard<std::mutex> lock(statsMutex);
  stats = DiagnosticStats{};
}

const DiagnosticInfo &DiagnosticManager::getDiagnosticInfo(DiagnosticID id) {
  size_t index = static_cast<size_t>(id);
  if (index >= static_cast<size_t>(DiagnosticID::NumDiagnostics)) {
    // Return a default error for invalid IDs
    static const DiagnosticInfo invalidDiag(
        DiagnosticLevel::Error, DiagnosticKind::System, "Invalid diagnostic id",
        "An invalid diagnostic id was used.");
    return invalidDiag;
  }

  return sDiagnosticInfos[index];
}

bool DiagnosticManager::shouldContinue() const {
  std::lock_guard<std::mutex> lock(statsMutex);

  // Don't continue if we have fatal errors
  if (stats.fatalCount > 0) {
    return false;
  }

  // Don't continue if we've hit the error limit
  if (maxErrors > 0 && stats.errorCount >= maxErrors) {
    return false;
  }

  return true;
}

void DiagnosticManager::printStats(std::ostream &OS) const {
  auto stats = getStats();

  OS << "Diagnostic Statistics:\n";
  OS << "  Notes: " << stats.noteCount << "\n";
  OS << "  Warnings: " << stats.warningCount << "\n";
  OS << "  Errors: " << stats.errorCount << "\n";
  OS << "  Fatal errors: " << stats.fatalCount << "\n";
  OS << "  Total: " << stats.diagnosticCount << "\n";
}

void DiagnosticManager::updateStats(DiagnosticLevel level) const {
  std::lock_guard<std::mutex> lock(statsMutex);

  switch (level) {
  case DiagnosticLevel::Note:
    ++stats.noteCount;
    break;
  case DiagnosticLevel::Warning:
    ++stats.warningCount;
    break;
  case DiagnosticLevel::Error:
    ++stats.errorCount;
    break;
  case DiagnosticLevel::Fatal:
    ++stats.fatalCount;
    break;
  }

  ++stats.diagnosticCount;
}

bool DiagnosticManager::shouldSuppress(const DiagnosticInfo &info) const {
  switch (info.Level) {
  case DiagnosticLevel::Note:
    return suppressNotes;
  case DiagnosticLevel::Warning:
    return suppressWarnings;
  case DiagnosticLevel::Error:
  case DiagnosticLevel::Fatal:
    return false; // Never suppress errors
  }
  return false;
}

// TextDiagnosticConsumer implementation
void TextDiagnosticConsumer::handleDiagnostic(const Diagnostic &diag,
                                              const DiagnosticInfo &info,
                                              const SourceManager *srcMgr) {
  // Format the location
  std::string location = "<unknown>";
  if (srcMgr && diag.getLocation().isValid()) {
    auto fullLoc = srcMgr->getFullLoc(diag.getLocation());
    if (fullLoc.isValid()) {
      std::ostringstream oss;
      oss << fullLoc.getFilename() << ":" << fullLoc.getLineNumber() << ":"
          << fullLoc.getColumnNumber();
      location = oss.str();
    }
  }

  // Format the level
  std::string levelStr;
  std::string colorStart, colorEnd;

  if (showColor) {
    colorEnd = "\033[0m"; // Reset color
  }

  switch (info.Level) {
  case DiagnosticLevel::Note:
    levelStr = "note";
    if (showColor)
      colorStart = "\033[36m"; // Cyan
    break;
  case DiagnosticLevel::Warning:
    levelStr = "warning";
    if (showColor)
      colorStart = "\033[33m"; // Yellow
    break;
  case DiagnosticLevel::Error:
    levelStr = "error";
    if (showColor)
      colorStart = "\033[31m"; // Red
    break;
  case DiagnosticLevel::Fatal:
    levelStr = "fatal error";
    if (showColor)
      colorStart = "\033[1;31m"; // Bold red
    break;
  }

  // Format the message
  std::string message =
      formatMessage(info.DetailedMessage, diag.getArguments());

  // Output the diagnostic
  os << location << ": " << colorStart << levelStr << colorEnd << ": "
     << message << "\n";

  // Print source line and highlighting if available
  if (srcMgr && diag.getLocation().isValid()) {
    printSourceLine(srcMgr, diag.getLocation(), diag.getRanges());
  }

  // Print fix-it hints
  for (const auto &hint : diag.getFixItHints()) {
    os << "  fix-it: replace with '" << hint.replacement << "'\n";
  }
}

void TextDiagnosticConsumer::printSourceLine(
    const SourceManager *srcMgr, SourceLocation loc,
    const std::vector<SourceRange> &ranges) {
  if (!srcMgr || loc.isInvalid())
    return;

  auto fullLoc = srcMgr->getFullLoc(loc);
  if (!fullLoc.isValid())
    return;

  // Get the line text (this is a simplified version)
  // In a real implementation, you'd extract the actual line from the source
  const char *lineStart = fullLoc.getCharacterData();
  if (!lineStart)
    return;

  // Find the beginning of the line
  while (lineStart >
             srcMgr->getFileEntry(fullLoc.getFileID())->getBufferStart() &&
         lineStart[-1] != '\n') {
    --lineStart;
  }

  // Find the end of the line
  const char *lineEnd = lineStart;
  while (*lineEnd && *lineEnd != '\n' && *lineEnd != '\r') {
    ++lineEnd;
  }

  // Print the line
  std::string line(lineStart, lineEnd);
  os << line << "\n";

  // Print highlighting
  uint32_t colNum = fullLoc.getColumnNumber();
  if (colNum > 0) {
    std::string highlight(colNum - 1, ' ');
    highlight += '^';

    // Add tildes for ranges
    for (const auto &range : ranges) {
      if (srcMgr->getFileID(range.getBegin()) == fullLoc.getFileID()) {
        uint32_t startCol = srcMgr->getColumnNumber(range.getBegin());
        uint32_t endCol = srcMgr->getColumnNumber(range.getEnd());

        if (startCol > 0 && endCol > startCol) {
          for (uint32_t i = startCol; i < endCol && i < highlight.size(); ++i) {
            if (highlight[i - 1] == ' ') {
              highlight[i - 1] = '~';
            }
          }
        }
      }
    }

    if (showColor) {
      os << "\033[32m" << highlight << "\033[0m\n"; // Green
    } else {
      os << highlight << "\n";
    }
  }
}

std::string
TextDiagnosticConsumer::formatMessage(std::string_view message,
                                      const std::vector<std::string> &args) {
  std::string result(message);

  // Replace %0, %1, etc. with arguments
  for (size_t i = 0; i < args.size(); ++i) {
    std::string placeholder = "%" + std::to_string(i);
    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
      result.replace(pos, placeholder.length(), args[i]);
      pos += args[i].length();
    }
  }

  return result;
}

// JSONDiagnosticConsumer implementation
void JSONDiagnosticConsumer::beginSourceFile() {
  os << "{\"diagnostics\": [";
  firstDiag = true;
}

void JSONDiagnosticConsumer::handleDiagnostic(const Diagnostic &diag,
                                              const DiagnosticInfo &info,
                                              const SourceManager *srcMgr) {
  if (!firstDiag) {
    os << ",";
  }
  firstDiag = false;

  os << "\n  {";

  // Diagnostic id
  os << "\"id\": " << static_cast<uint32_t>(diag.getID()) << ",";

  // Level
  os << "\"level\": \"";
  switch (info.Level) {
  case DiagnosticLevel::Note:
    os << "note";
    break;
  case DiagnosticLevel::Warning:
    os << "warning";
    break;
  case DiagnosticLevel::Error:
    os << "error";
    break;
  case DiagnosticLevel::Fatal:
    os << "fatal";
    break;
  }
  os << "\",";

  // Message
  std::string message = std::string(info.DetailedMessage);

  // Replace %0, %1, etc. with arguments
  for (size_t i = 0; i < diag.getArguments().size(); ++i) {
    std::string placeholder = "%" + std::to_string(i);
    size_t pos = 0;
    while ((pos = message.find(placeholder, pos)) != std::string::npos) {
      message.replace(pos, placeholder.length(), diag.getArguments()[i]);
      pos += diag.getArguments()[i].length();
    }
  }
  os << "\"message\": \"" << message << "\",";

  // location
  if (srcMgr && diag.getLocation().isValid()) {
    auto fullLoc = srcMgr->getFullLoc(diag.getLocation());
    if (fullLoc.isValid()) {
      os << "\"location\": {";
      os << "\"file\": \"" << fullLoc.getFilename() << "\",";
      os << "\"line\": " << fullLoc.getLineNumber() << ",";
      os << "\"column\": " << fullLoc.getColumnNumber();
      os << "}";
    } else {
      os << "\"location\": null";
    }
  } else {
    os << "\"location\": null";
  }

  os << "}";
}

void JSONDiagnosticConsumer::endSourceFile() { os << "\n]}\n"; }

} // namespace ml