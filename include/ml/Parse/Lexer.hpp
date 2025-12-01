#pragma once

#include "ml/Basic/SourceLocation.hpp"
#include "ml/Basic/StringInterner.hpp"
#include "ml/Managers/DiagnosticManager.hpp"
#include "ml/Parse/Token.hpp"
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace ml {

class SourceManager;

/// Statistics about lexer performance
struct LexerStats {
  size_t tokenCount = 0;
  size_t identifierCount = 0;
  size_t keywordCount = 0;
  size_t literalCount = 0;
  size_t commentCount = 0;
  size_t lineCount = 0;
  size_t characterCount = 0;
  double lexingTimeMs = 0.0;

  // Performance metrics
  size_t simdOperations = 0;
  size_t lookupTableHits = 0;
  size_t branchMisses = 0;
  double avgTokenLength = 0.0;

  void updateAverages() {
    if (tokenCount > 0) {
      avgTokenLength = static_cast<double>(characterCount) / tokenCount;
    }
  }
};

/// Lexer options and configuration
struct LexerOptions {
  bool retainComments = false;         // Keep comment tokens
  bool retainWhitespace = false;       // Keep whitespace tokens
  bool allowUnicodeIdentifiers = true; // Allow Unicode in identifiers
  bool warningsAsErrors = false;       // Treat lexer warnings as errors
  bool strictMode = false;             // Strict language mode

  // Performance options
  bool enableSimdOptimizations = false; // Use SIMD instructions when available
  bool enableLookupTables =
      true; // Use lookup tables for character classification
  bool enablePrefetching = true; // Enable memory prefetching
  bool enableFastPath = true;    // Enable fast paths for common tokens

  // Buffer management
  size_t readAheadSize = 4096;     // Read-ahead buffer size
  bool enableMemoryMapping = true; // Use memory mapping for large files

  // Character encoding
  enum class Encoding { UTF8, ASCII, Latin1 } inputEncoding = Encoding::UTF8;
};

/// Callback for handling preprocessor directives
using PreprocessorCallback =
    std::function<void(std::string_view directive, SourceLocation loc)>;

/// Main lexer class for tokenizing source code
class Lexer {
public:
  Lexer(const SourceManager &srcMgr, FileID fileID, StringInterner &interner,
        DiagnosticManager &diagMgr, const LexerOptions &opts = LexerOptions{});

  Lexer(std::string_view source, StringInterner &interner,
        DiagnosticManager &diagMgr, const LexerOptions &opts = LexerOptions{});

  ~Lexer();

  // Non-copyable but movable
  Lexer(const Lexer &) = delete;
  Lexer &operator=(const Lexer &) = delete;
  Lexer(Lexer &&) noexcept;
  Lexer &operator=(Lexer &&) noexcept;

  /// Tokenize the next token
  Token nextToken();

  /// Peek at the next token without consuming it
  const Token &peekToken();

  /// Check if we've reached the end of the file
  bool isAtEnd() const;

  /// Get current position in the source
  SourceLocation getCurrentLocation() const;

  /// Get the current line number
  uint32_t getCurrentLine() const { return currentLine; }

  /// Get the current column number
  uint32_t getCurrentColumn() const;

  /// Skip to the end of the current line
  void skipToEndOfLine();

  /// Skip whitespace and comments
  void skipTrivial();

  /// Skip whitespace and comments (optimized version)
  void skipTrivialOptimized();

  /// Reset to the beginning of the source
  void reset();

  /// Set preprocessor directive callback
  void setPreprocessorCallback(PreprocessorCallback callback) {
    ppCallback = std::move(callback);
  }

  /// Get lexer statistics
  LexerStats getStats() const;

  /// Get lexer options
  const LexerOptions &getOptions() const { return options; }

  /// Get the source text
  std::string_view getSourceText() const { return source; }

  /// Get the FileID (if lexing from SourceManager)
  FileID getFileID() const { return fid; }

  void printStats(std::ostream &os) const;

private:
  const SourceManager *srcMgr;
  FileID fid;
  StringInterner &interner;
  DiagnosticManager &diagMgr;
  LexerOptions options;
  PreprocessorCallback ppCallback;

  // Source text and position
  std::string_view source;
  const char *current;
  const char *end;
  const char *lineStart;
  uint32_t currentLine;
  SourceLocation baseLocation;

  // Lookahead token for peeking
  mutable std::unique_ptr<Token> peekedToken;
  mutable bool hasPeekedToken = false;

  // Statistics
  mutable LexerStats stats; // Helper methods
  char peek(size_t offset = 0) const;
  char advance();
  bool isAtEnd(const char *pos) const { return pos >= end; }
  bool match(char expected);
  bool match(const char *expected);

  // Character classification
  bool isAlpha(char c) const;
  bool isDigit(char c) const;
  bool isAlnum(char c) const;
  bool isHexDigit(char c) const;
  bool isBinaryDigit(char c) const;
  bool isOctalDigit(char c) const;
  bool isWhitespace(char c) const;
  bool isNewline(char c) const;

  // Token creation
  Token makeToken(TokenKind kind) const;
  Token makeToken(TokenKind kind, uint32_t length) const;
  Token makeToken(TokenKind kind, const char *start, const char *end) const;
  Token makeIdentifierToken(const char *start, const char *end);
  Token makeStringToken(const char *start, const char *end);
  Token makeCharToken(const char *start, const char *end);
  Token makeNumberToken(const char *start, const char *end);

  // Specific token lexing
  Token lexIdentifier();
  Token lexNumber();
  Token lexString(char quote);
  Token lexCharLiteral();
  Token lexComment();
  Token lexOperator();

  // Utility methods
  void skipLineComment();
  void skipBlockComment();
  void skipWhitespace();
  void handleNewline();
  SourceLocation getLocationAt(const char *pos) const;
  void reportError(DiagnosticID id, SourceLocation loc);
  void reportError(DiagnosticID id, SourceLocation loc, std::string_view arg);
  void reportError(DiagnosticID id, SourceLocation loc,
                   std::string_view expected, std::string_view actual);

  // String processing
  std::string processStringLiteral(std::string_view raw) const;
  char processCharLiteral(std::string_view raw) const;
  char processEscapeSequence(const char *&pos) const;

  // Number processing
  TokenKind classifyNumber(std::string_view text) const;
  bool isValidIntegerSuffix(std::string_view suffix) const;
  bool isValidFloatSuffix(std::string_view suffix) const;
};

/// Token manager for efficient token storage and retrieval
class TokenManager {
public:
  explicit TokenManager(size_t initialCapacity = 1024);
  ~TokenManager();

  // Non-copyable but movable
  TokenManager(const TokenManager &) = delete;
  TokenManager &operator=(const TokenManager &) = delete;
  TokenManager(TokenManager &&) noexcept;
  TokenManager &operator=(TokenManager &&) noexcept;

  /// Add a token to the manager
  void addToken(const Token &token);
  void addToken(Token &&token);

  /// Get token by index
  const Token &getToken(size_t index) const;
  Token &getToken(size_t index);

  /// Get number of tokens
  size_t getTokenCount() const { return tokens.size(); }

  /// Check if empty
  bool empty() const { return tokens.empty(); }

  /// Clear all tokens
  void clear();

  /// Reserve capacity for tokens
  void reserve(size_t capacity) { tokens.reserve(capacity); }

  /// Get iterator access
  std::vector<Token>::iterator begin() { return tokens.begin(); }
  std::vector<Token>::iterator end() { return tokens.end(); }
  std::vector<Token>::const_iterator begin() const { return tokens.begin(); }
  std::vector<Token>::const_iterator end() const { return tokens.end(); }
  std::vector<Token>::const_iterator cbegin() const { return tokens.cbegin(); }
  std::vector<Token>::const_iterator cend() const { return tokens.cend(); }

  /// Find tokens by location
  std::vector<size_t> findTokensInRange(SourceRange range) const;
  size_t findTokenAtLocation(SourceLocation loc) const;

  /// Get tokens by kind
  std::vector<size_t> findTokensByKind(TokenKind kind) const;

  /// Token stream operations
  class TokenStream {
  public:
    explicit TokenStream(const TokenManager &mgr) : manager(mgr), index(0) {}

    const Token &current() const;
    const Token &peek(size_t offset = 1) const;
    void advance();
    bool isAtEnd() const { return index >= manager.getTokenCount(); }
    void reset() { index = 0; }
    size_t getIndex() const { return index; }
    void setIndex(size_t index) { this->index = index; }

  private:
    const TokenManager &manager;
    size_t index;
  };

  /// Create a token stream
  TokenStream createStream() const { return TokenStream(*this); }

  /// Print all tokens (for debugging)
  void printTokens(std::ostream &os) const;

  /// Get memory usage
  size_t getMemoryUsage() const;

private:
  std::vector<Token> tokens;

  // For efficient location-based searches
  mutable bool locationIndexValid = false;
  mutable std::vector<size_t> locationIndex;
  void buildLocationIndex() const;
};

/// Convenience function to tokenize a string
std::vector<Token> tokenizeString(std::string_view source,
                                  StringInterner &interner,
                                  DiagnosticManager &diagMgr,
                                  const LexerOptions &opts = LexerOptions{});

/// Convenience function to tokenize a file
std::vector<Token> tokenizeFile(const SourceManager &srcMgr, FileID fileID,
                                StringInterner &interner,
                                DiagnosticManager &diagMgr,
                                const LexerOptions &opts = LexerOptions{});

/// High-performance batch tokenization for large sources
class BatchTokenizer {
public:
  BatchTokenizer(StringInterner &interner, DiagnosticManager &diagMgr,
                 const LexerOptions &opts = LexerOptions{});

  /// Tokenize multiple sources in parallel (if supported)
  std::vector<std::vector<Token>>
  tokenizeParallel(const std::vector<std::string_view> &sources);

  /// Tokenize with streaming support for very large files
  void tokenizeStreaming(std::string_view source,
                         std::function<void(const Token &)> callback);

  /// Get aggregate statistics from all tokenization operations
  LexerStats getAggregateStats() const { return aggregateStats; }

private:
  StringInterner &interner;
  DiagnosticManager &diagMgr;
  LexerOptions options;
  mutable LexerStats aggregateStats;
};

} // namespace ml