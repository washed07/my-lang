#include "ml/Parse/Lexer.hpp"
#include "ml/Managers/SourceManager.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>

#ifdef _MSC_VER
#include <immintrin.h>
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <x86intrin.h>
#endif

#include <array>
#include <bit>
#include <cstdint>

namespace ml {

// Optimized lookup tables for character classification
static constexpr std::array<uint8_t, 256> CHAR_CLASS_TABLE = []() {
  std::array<uint8_t, 256> table{};

  // Bit flags for character classes
  constexpr uint8_t ALPHA = 1;
  constexpr uint8_t DIGIT = 2;
  constexpr uint8_t WHITESPACE = 4;
  constexpr uint8_t NEWLINE = 8;
  constexpr uint8_t HEX = 16;

  // Initialize alpha characters
  for (int c = 'a'; c <= 'z'; ++c) {
    table[c] = ALPHA | (c <= 'f' ? HEX : 0);
  }
  for (int c = 'A'; c <= 'Z'; ++c) {
    table[c] = ALPHA | (c <= 'F' ? HEX : 0);
  }

  // Initialize digits
  for (int c = '0'; c <= '9'; ++c) {
    table[c] = DIGIT | HEX;
  }

  // Whitespace characters
  table[' '] = WHITESPACE;
  table['\t'] = WHITESPACE;
  table['\v'] = WHITESPACE;
  table['\f'] = WHITESPACE;

  // Newline characters
  table['\n'] = NEWLINE;
  table['\r'] = NEWLINE;

  // Underscore is alpha for identifiers
  table['_'] = ALPHA;

  return table;
}();

// Fast character classification using lookup table
inline bool isAlphaFast(unsigned char c) { return CHAR_CLASS_TABLE[c] & 1; }
inline bool isDigitFast(unsigned char c) { return CHAR_CLASS_TABLE[c] & 2; }
inline bool isAlnumFast(unsigned char c) { return CHAR_CLASS_TABLE[c] & 3; }
inline bool isWhitespaceFast(unsigned char c) {
  return CHAR_CLASS_TABLE[c] & 4;
}
inline bool isNewlineFast(unsigned char c) { return CHAR_CLASS_TABLE[c] & 8; }
inline bool isHexDigitFast(unsigned char c) { return CHAR_CLASS_TABLE[c] & 16; }

// SIMD-optimized whitespace skipping
static std::pair<const char *, size_t>
skipWhitespaceSimdWithStats(const char *ptr, const char *end) {
  size_t simdOps = 0;
#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
  // Use AVX2 for faster whitespace skipping
  const __m256i whitespace = _mm256_set1_epi8(' ');
  const __m256i tab = _mm256_set1_epi8('\t');
  const __m256i vtab = _mm256_set1_epi8('\v');
  const __m256i ff = _mm256_set1_epi8('\f');

  while (ptr + 32 <= end) {
    __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(ptr));

    __m256i is_space = _mm256_cmpeq_epi8(chunk, whitespace);
    __m256i is_tab = _mm256_cmpeq_epi8(chunk, tab);
    __m256i is_vtab = _mm256_cmpeq_epi8(chunk, vtab);
    __m256i is_ff = _mm256_cmpeq_epi8(chunk, ff);

    __m256i is_whitespace = _mm256_or_si256(_mm256_or_si256(is_space, is_tab),
                                            _mm256_or_si256(is_vtab, is_ff));

    uint32_t mask = _mm256_movemask_epi8(is_whitespace);

    ++simdOps;
    if (mask != 0xFFFFFFFF) {
      // Found non-whitespace, find first occurrence
      int offset = __builtin_ctz(~mask);
      return {ptr + offset, simdOps};
    }
    ptr += 32;
  }
#elif defined(__SSE2__) || (defined(_MSC_VER) && defined(__SSE2__))
  // Use SSE2 for faster whitespace skipping
  const __m128i whitespace = _mm_set1_epi8(' ');
  const __m128i tab = _mm_set1_epi8('\t');
  const __m128i vtab = _mm_set1_epi8('\v');
  const __m128i ff = _mm_set1_epi8('\f');

  while (ptr + 16 <= end) {
    __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));

    __m128i is_space = _mm_cmpeq_epi8(chunk, whitespace);
    __m128i is_tab = _mm_cmpeq_epi8(chunk, tab);
    __m128i is_vtab = _mm_cmpeq_epi8(chunk, vtab);
    __m128i is_ff = _mm_cmpeq_epi8(chunk, ff);

    __m128i is_whitespace = _mm_or_si128(_mm_or_si128(is_space, is_tab),
                                         _mm_or_si128(is_vtab, is_ff));

    uint32_t mask = _mm_movemask_epi8(is_whitespace);

    ++simdOps;
    if (mask != 0xFFFF) {
      // Found non-whitespace, find first occurrence
      int offset = __builtin_ctz(~mask);
      return {ptr + offset, simdOps};
    }
    ptr += 16;
  }
#endif

  // Fallback to scalar processing
  while (ptr < end && isWhitespaceFast(*ptr)) {
    ++ptr;
  }
  return {ptr, simdOps};
}

// Backward compatibility wrapper
static const char *skipWhitespaceSimd(const char *ptr, const char *end) {
  return skipWhitespaceSimdWithStats(ptr, end).first;
}

// Optimized identifier/keyword lookup using perfect hash or trie
static constexpr std::array<std::pair<std::string_view, TokenKind>, 27>
    KEYWORD_TABLE = {{{"auto", TokenKind::Auto},
                      {"break", TokenKind::Break},
                      {"case", TokenKind::Case},
                      {"const", TokenKind::Const},
                      {"continue", TokenKind::Continue},
                      {"default", TokenKind::Default},
                      {"do", TokenKind::Do},
                      {"else", TokenKind::Else},
                      {"enum", TokenKind::Enum},
                      {"extern", TokenKind::Extern},
                      {"false", TokenKind::False},
                      {"for", TokenKind::For},
                      {"fn", TokenKind::Fn},
                      {"if", TokenKind::If},
                      {"import", TokenKind::Import},
                      {"let", TokenKind::Let},
                      {"mod", TokenKind::Mod},
                      {"mut", TokenKind::Mut},
                      {"null", TokenKind::Null},
                      {"return", TokenKind::Return},
                      {"struct", TokenKind::Struct},
                      {"switch", TokenKind::Switch},
                      {"true", TokenKind::True},
                      {"type", TokenKind::Type},
                      {"var", TokenKind::Var},
                      {"while", TokenKind::While}}};

// Fast keyword lookup using binary search on sorted table
static TokenKind getKeywordKindFast(std::string_view text) {
  auto it = std::lower_bound(
      KEYWORD_TABLE.begin(), KEYWORD_TABLE.end(), text,
      [](const auto &pair, std::string_view key) { return pair.first < key; });

  if (it != KEYWORD_TABLE.end() && it->first == text) {
    return it->second;
  }
  return TokenKind::Identifier;
}

// Lexer implementation
Lexer::Lexer(const SourceManager &srcMgr, FileID fileID,
             StringInterner &interner, DiagnosticManager &diagMgr,
             const LexerOptions &opts)
    : srcMgr(&srcMgr), fid(fileID), interner(interner), diagMgr(diagMgr),
      options(opts), currentLine(1),
      baseLocation(srcMgr.getLocForStartOfFile(fileID)) {

  const FileEntry *entry = srcMgr.getFileEntry(fileID);
  if (entry) {
    source = std::string_view(entry->getBufferStart(), entry->getSize());
    current = source.data();
    end = current + source.size();
    lineStart = current;
  } else {
    current = end = lineStart = nullptr;
  }
}

Lexer::Lexer(std::string_view source, StringInterner &interner,
             DiagnosticManager &diagMgr, const LexerOptions &opts)
    : srcMgr(nullptr), fid(FileID::getInvalid()), interner(interner),
      diagMgr(diagMgr), options(opts), source(source), currentLine(1),
      baseLocation(SourceLocation::getInvalidLoc()) {

  current = source.data();
  end = current + source.size();
  lineStart = current;
}

Lexer::~Lexer() = default;

Lexer::Lexer(Lexer &&other) noexcept
    : srcMgr(other.srcMgr), fid(other.fid), interner(other.interner),
      diagMgr(other.diagMgr), options(other.options),
      ppCallback(std::move(other.ppCallback)), source(other.source),
      current(other.current), end(other.end), lineStart(other.lineStart),
      currentLine(other.currentLine), baseLocation(other.baseLocation),
      peekedToken(std::move(other.peekedToken)),
      hasPeekedToken(other.hasPeekedToken), stats(other.stats) {

  other.current = other.end = other.lineStart = nullptr;
}

Token Lexer::nextToken() {
  // If we have a peeked token, return it
  if (hasPeekedToken) {
    hasPeekedToken = false;
    Token result = *peekedToken;
    peekedToken.reset();
    return result;
  }

  auto start = std::chrono::high_resolution_clock::now();

  // Skip trivial if not retaining it - optimized fast path
  if (!options.retainWhitespace && !options.retainComments) {
    skipTrivialOptimized();
  }

  // Check for end of file
  if (isAtEnd()) {
    ++stats.tokenCount;
    stats.updateAverages();
    return makeToken(TokenKind::EndOfFile);
  }

  // Safety check to prevent infinite loops
  const char *startPos = current;

  // Mark if we're at the start of a line
  bool atStartOfLine = (current == lineStart);

  // Prefetch next cache line if enabled
  if (options.enablePrefetching && current + 64 < end) {
#ifdef _MSC_VER
    _mm_prefetch(current + 64, _MM_HINT_T0);
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(current + 64, 0, 3);
#endif
  }

  // Fast character classification using direct memory access
  unsigned char c = *current;

  Token token;

  // Track lookup table usage if enabled
  if (options.enableLookupTables) {
    ++stats.lookupTableHits; // Optimized token dispatch using lookup table
  }

  if (options.enableFastPath) {
    // Fast path for most common tokens using bit manipulation
    if ((options.enableLookupTables ? isAlphaFast(c) : isAlpha(c)) ||
        c == '_') {
      token = lexIdentifier();
    } else if ((options.enableLookupTables ? isDigitFast(c) : isDigit(c))) {
      token = lexNumber();
    } else if ((options.enableLookupTables ? isWhitespaceFast(c)
                                           : isWhitespace(c))) {
      if (options.retainWhitespace) {
        const char *start = current;
        skipWhitespace();
        token = makeToken(TokenKind::Whitespace, start, current);
      } else {
        skipWhitespace();
        return nextToken();
      }
    } else if (isNewlineFast(c)) {
      if (options.retainWhitespace) {
        const char *start = current;
        handleNewline();
        token = makeToken(TokenKind::Newline, start, current);
      } else {
        handleNewline();
        return nextToken();
      }
    } else if (c == '"' || c == '\'') {
      if (c == '"') {
        token = lexString('"');
      } else {
        token = lexCharLiteral();
      }
    } else if (c == '/' && current + 1 < end &&
               (current[1] == '/' || current[1] == '*')) {
      if (options.retainComments) {
        token = lexComment();
      } else {
        if (current[1] == '/') {
          skipLineComment();
        } else {
          skipBlockComment();
        }
        return nextToken();
      }
    } else {
      token = lexOperator();
    }
  } else {
    // Fallback to original logic
    if (isWhitespace(c)) {
      if (options.retainWhitespace) {
        const char *start = current;
        skipWhitespace();
        token = makeToken(TokenKind::Whitespace, start, current);
      } else {
        skipWhitespace();
        return nextToken();
      }
    } else if (isNewline(c)) {
      if (options.retainWhitespace) {
        const char *start = current;
        handleNewline();
        token = makeToken(TokenKind::Newline, start, current);
      } else {
        handleNewline();
        return nextToken();
      }
    } else if (isAlpha(c) || c == '_') {
      token = lexIdentifier();
    } else if (isDigit(c)) {
      token = lexNumber();
    } else if (c == '"' || c == '\'') {
      if (c == '"') {
        token = lexString('"');
      } else {
        token = lexCharLiteral();
      }
    } else if (c == '/' && (peek(1) == '/' || peek(1) == '*')) {
      if (options.retainComments) {
        token = lexComment();
      } else {
        if (peek(1) == '/') {
          skipLineComment();
        } else {
          skipBlockComment();
        }
        return nextToken();
      }
    } else {
      token = lexOperator();
    }
  }

  // Set flags
  if (atStartOfLine) {
    token.addFlag(TokenFlags::AtStartOfLine);
  }

  ++stats.tokenCount;

  // Safety check: ensure we always advance position
  if (current == startPos && !isAtEnd()) {
    ++current;
    ++stats.characterCount;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  stats.lexingTimeMs += duration.count() / 1000.0;

  return token;
}

const Token &Lexer::peekToken() {
  if (!hasPeekedToken) {
    peekedToken = std::make_unique<Token>(nextToken());
    hasPeekedToken = true;
  }
  return *peekedToken;
}

bool Lexer::isAtEnd() const { return current >= end; }

SourceLocation Lexer::getCurrentLocation() const {
  return getLocationAt(current);
}

uint32_t Lexer::getCurrentColumn() const {
  return static_cast<uint32_t>(current - lineStart + 1);
}

void Lexer::skipToEndOfLine() {
  while (!isAtEnd() && !isNewline(peek())) {
    advance();
  }
}

void Lexer::skipTrivial() {
  while (!isAtEnd()) {
    char c = peek();
    if (isWhitespace(c)) {
      skipWhitespace();
    } else if (isNewline(c)) {
      handleNewline();
    } else if (c == '/' && (peek(1) == '/' || peek(1) == '*')) {
      if (peek(1) == '/') {
        skipLineComment();
      } else {
        skipBlockComment();
      }
    } else {
      break;
    }
  }
}

void Lexer::skipTrivialOptimized() {
  const char *ptr = current;

  // Fast skip using SIMD when possible
  while (ptr < end) {
    unsigned char c = *ptr;

    if ((options.enableLookupTables ? isWhitespaceFast(c) : isWhitespace(c))) {
      // Use SIMD optimized whitespace skipping if enabled
      if (options.enableSimdOptimizations) {
        auto [newPtr, simdOps] = skipWhitespaceSimdWithStats(ptr, end);
        stats.characterCount += (newPtr - current);
        stats.simdOperations += simdOps;
        current = newPtr;
        ptr = newPtr;
      } else {
        // Fallback to simple character-by-character skipping
        while (ptr < end && (options.enableLookupTables ? isWhitespaceFast(*ptr)
                                                        : isWhitespace(*ptr))) {
          ++ptr;
        }
        stats.characterCount += (ptr - current);
        current = ptr;
      }
    } else if ((options.enableLookupTables ? isNewlineFast(c) : isNewline(c))) {
      handleNewline();
      ptr = current;
    } else if (c == '/' && ptr + 1 < end) {
      char next = ptr[1];
      if (next == '/') {
        // Skip line comment
        ptr += 2;
        while (ptr < end && !isNewlineFast(*ptr))
          ++ptr;
        stats.characterCount += (ptr - current);
        current = ptr;
      } else if (next == '*') {
        // Skip block comment - need careful handling for newlines
        ptr += 2;
        while (ptr + 1 < end) {
          if (ptr[0] == '*' && ptr[1] == '/') {
            ptr += 2;
            break;
          }
          if (isNewlineFast(*ptr)) {
            stats.characterCount += (ptr - current);
            current = ptr;
            handleNewline();
            ptr = current;
          } else {
            ++ptr;
          }
        }
        stats.characterCount += (ptr - current);
        current = ptr;
      } else {
        break;
      }
    } else {
      break;
    }
  }
}

void Lexer::reset() {
  current = source.data();
  lineStart = current;
  currentLine = 1;
  hasPeekedToken = false;
  peekedToken.reset();
  stats = LexerStats{};
}

LexerStats Lexer::getStats() const {
  stats.characterCount = source.size();
  stats.lineCount = currentLine;
  return stats;
}

// Helper methods
char Lexer::peek(size_t offset) const {
  const char *pos = current + offset;
  return isAtEnd(pos) ? '\0' : *pos;
}

char Lexer::advance() {
  if (isAtEnd())
    return '\0';
  char c = *current++;
  ++stats.characterCount;
  return c;
}

bool Lexer::match(char expected) {
  if (peek() != expected)
    return false;
  advance();
  return true;
}

bool Lexer::match(const char *expected) {
  size_t len = std::strlen(expected);
  if (current + len > end)
    return false;

  if (std::strncmp(current, expected, len) == 0) {
    current += len;
    return true;
  }
  return false;
}

// Character classification - optimized with lookup tables
bool Lexer::isAlpha(char c) const {
  if (options.enableLookupTables) {
    return isAlphaFast(static_cast<unsigned char>(c));
  } else {
    // Fallback to standard character checks
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  }
}

bool Lexer::isDigit(char c) const {
  if (options.enableLookupTables) {
    return isDigitFast(static_cast<unsigned char>(c));
  } else {
    // Fallback to standard character checks
    return c >= '0' && c <= '9';
  }
}

bool Lexer::isAlnum(char c) const {
  return isAlnumFast(static_cast<unsigned char>(c));
}

bool Lexer::isHexDigit(char c) const {
  return isHexDigitFast(static_cast<unsigned char>(c));
}

bool Lexer::isBinaryDigit(char c) const { return c == '0' || c == '1'; }

bool Lexer::isOctalDigit(char c) const { return c >= '0' && c <= '7'; }

bool Lexer::isWhitespace(char c) const {
  if (options.enableLookupTables) {
    return isWhitespaceFast(static_cast<unsigned char>(c));
  } else {
    // Fallback to standard character checks
    return c == ' ' || c == '\t' || c == '\v' || c == '\f';
  }
}

bool Lexer::isNewline(char c) const {
  if (options.enableLookupTables) {
    return isNewlineFast(static_cast<unsigned char>(c));
  } else {
    // Fallback to standard character checks
    return c == '\n' || c == '\r';
  }
}

// Token creation
Token Lexer::makeToken(TokenKind kind) const {
  return Token(kind, getCurrentLocation(), 0);
}

Token Lexer::makeToken(TokenKind kind, uint32_t length) const {
  SourceLocation loc = getLocationAt(current - length);
  return Token(kind, loc, length);
}

Token Lexer::makeToken(TokenKind kind, const char *start,
                       const char *end) const {
  uint32_t length = static_cast<uint32_t>(end - start);
  SourceLocation loc = getLocationAt(start);
  return Token(kind, loc, length);
}

Token Lexer::makeIdentifierToken(const char *start, const char *end) {
  std::string_view text(start, end - start);

  // Check if it's a keyword using optimized lookup
  TokenKind kind = getKeywordKindFast(text);

  Token token = makeToken(kind, start, end);

  if (kind == TokenKind::Identifier) {
    token.setText(interner.intern(text));
    ++stats.identifierCount;
  } else {
    token.addFlag(TokenFlags::IsKeyword);
    ++stats.keywordCount;
  }

  return token;
}

// Lexing methods
Token Lexer::lexIdentifier() {
  const char *start = current;

  // First character must be alpha or underscore (already validated by caller)
  ++current;
  ++stats.characterCount;

  // Fast scan for alphanumeric characters and underscores
  // Use branchless scanning when possible
  const char *ptr = current;
  while (ptr < end) {
    unsigned char c = *ptr;
    if (!isAlnumFast(c) && c != '_') {
      break;
    }
    ++ptr;
  }

  stats.characterCount += (ptr - current);
  current = ptr;

  return makeIdentifierToken(start, current);
}

Token Lexer::lexNumber() {
  const char *start = current;
  TokenKind kind = TokenKind::Integer;

  // Fast number scanning with reduced branching
  if (*current == '0' && current + 1 < end) {
    char next = current[1];
    if (next == 'x' || next == 'X') {
      // Hexadecimal
      current += 2;
      stats.characterCount += 2;
      const char *ptr = current;
      while (ptr < end && isHexDigitFast(*ptr))
        ++ptr;
      stats.characterCount += (ptr - current);
      current = ptr;
    } else if (next == 'b' || next == 'B') {
      // Binary
      current += 2;
      stats.characterCount += 2;
      const char *ptr = current;
      while (ptr < end && (*ptr == '0' || *ptr == '1'))
        ++ptr;
      stats.characterCount += (ptr - current);
      current = ptr;
    } else {
      // Octal or decimal starting with 0
      ++current;
      ++stats.characterCount;
      const char *ptr = current;
      while (ptr < end && *ptr >= '0' && *ptr <= '7')
        ++ptr;
      stats.characterCount += (ptr - current);
      current = ptr;
    }
  } else {
    // Decimal number - fast scan
    const char *ptr = current;
    while (ptr < end && isDigitFast(*ptr))
      ++ptr;
    stats.characterCount += (ptr - current);
    current = ptr;
  }

  // Check for decimal point (floating point)
  if (current < end && *current == '.' && current + 1 < end &&
      isDigitFast(current[1])) {
    kind = TokenKind::Float;
    ++current; // consume '.'
    ++stats.characterCount;

    // Scan fractional part
    const char *ptr = current;
    while (ptr < end && isDigitFast(*ptr))
      ++ptr;
    stats.characterCount += (ptr - current);
    current = ptr;

    // Exponent
    if (current < end && (*current == 'e' || *current == 'E')) {
      ++current;
      ++stats.characterCount;
      if (current < end && (*current == '+' || *current == '-')) {
        ++current;
        ++stats.characterCount;
      }
      const char *ptr = current;
      while (ptr < end && isDigitFast(*ptr))
        ++ptr;
      stats.characterCount += (ptr - current);
      current = ptr;
    }
  }

  // Suffix scanning (u, l, f, etc.) - fast scan
  const char *ptr = current;
  while (ptr < end && isAlphaFast(*ptr))
    ++ptr;
  stats.characterCount += (ptr - current);
  current = ptr;

  std::string_view text(start, current - start);
  Token token = makeToken(kind, start, current);
  token.setText(interner.intern(text));

  ++stats.literalCount;
  return token;
}

Token Lexer::lexString(char quote) {
  const char *start = current;
  ++current; // consume opening quote
  ++stats.characterCount;

  // Fast string scanning with proper escape sequence handling
  const char *ptr = current;
  bool hasEscapes = false;

  while (ptr < end && *ptr != quote) {
    if (*ptr == '\\') {
      hasEscapes = true;
      ptr++; // Skip backslash
      if (ptr >= end)
        break;

      // Handle specific escape sequences properly
      char escaped = *ptr;
      if (escaped == 'x') {
        // Hexadecimal escape \xnn
        ptr++; // Skip 'x'
        // Skip up to 2 hex digits
        for (int i = 0; i < 2 && ptr < end; i++) {
          if ((*ptr >= '0' && *ptr <= '9') || (*ptr >= 'A' && *ptr <= 'F') ||
              (*ptr >= 'a' && *ptr <= 'f')) {
            ptr++;
          } else {
            break;
          }
        }
      } else if (escaped == 'u') {
        // Unicode escape \uxxxx
        ptr++; // Skip 'u'
        // Skip exactly 4 hex digits
        for (int i = 0; i < 4 && ptr < end; i++) {
          if ((*ptr >= '0' && *ptr <= '9') || (*ptr >= 'A' && *ptr <= 'F') ||
              (*ptr >= 'a' && *ptr <= 'f')) {
            ptr++;
          } else {
            break;
          }
        }
      } else if (escaped == 'U') {
        // Unicode escape \Uxxxxxxxx
        ptr++; // Skip 'U'
        // Skip exactly 8 hex digits
        for (int i = 0; i < 8 && ptr < end; i++) {
          if ((*ptr >= '0' && *ptr <= '9') || (*ptr >= 'A' && *ptr <= 'F') ||
              (*ptr >= 'a' && *ptr <= 'f')) {
            ptr++;
          } else {
            break;
          }
        }
      } else if (escaped >= '0' && escaped <= '7') {
        // Octal escape \nnn
        ptr++; // Skip first octal digit
        // Skip up to 2 more octal digits
        for (int i = 0; i < 2 && ptr < end && *ptr >= '0' && *ptr <= '7'; i++) {
          ptr++;
        }
      } else {
        // Simple escape sequence (\\, \n, \t, etc.)
        ptr++;
      }
    } else if (*ptr == '\n' || *ptr == '\r') {
      // Unterminated string (newline in string)
      break;
    } else {
      ++ptr;
    }
  }

  if (ptr >= end) {
    reportError(DiagnosticID::UnterminatedStringLiteralError,
                getLocationAt(start));
    stats.characterCount += (ptr - current);
    current = ptr;
  } else {
    stats.characterCount += (ptr - current + 1); // Include closing quote
    current = ptr + 1;                           // consume closing quote
  }

  std::string_view text(start, current - start);
  Token token = makeToken(TokenKind::String, start, current);

  // Only mark for cleaning if we found escape sequences
  if (hasEscapes) {
    token.addFlag(TokenFlags::NeedsCleaning);
  }

  token.setText(interner.intern(text));

  ++stats.literalCount;
  return token;
}

Token Lexer::lexCharLiteral() {
  const char *start = current;
  ++current; // consume opening quote
  ++stats.characterCount;

  bool hasEscape = false;
  int charCount = 0;

  if (current < end && *current != '\'') {
    if (*current == '\\') {
      hasEscape = true;
      current++; // consume backslash
      charCount++;

      if (current < end) {
        char escaped = *current;
        current++; // consume escaped character
        charCount++;

        // Handle multi-character escape sequences
        if (escaped == 'x') {
          // Hexadecimal escape \xnn - consume up to 2 hex digits
          for (int i = 0; i < 2 && current < end; i++) {
            if ((*current >= '0' && *current <= '9') ||
                (*current >= 'A' && *current <= 'F') ||
                (*current >= 'a' && *current <= 'f')) {
              current++;
              charCount++;
            } else {
              break;
            }
          }
        } else if (escaped == 'u') {
          // Unicode escape \uxxxx - consume exactly 4 hex digits
          for (int i = 0; i < 4 && current < end; i++) {
            if ((*current >= '0' && *current <= '9') ||
                (*current >= 'A' && *current <= 'F') ||
                (*current >= 'a' && *current <= 'f')) {
              current++;
              charCount++;
            } else {
              break;
            }
          }
        } else if (escaped == 'U') {
          // Unicode escape \Uxxxxxxxx - consume exactly 8 hex digits
          for (int i = 0; i < 8 && current < end; i++) {
            if ((*current >= '0' && *current <= '9') ||
                (*current >= 'A' && *current <= 'F') ||
                (*current >= 'a' && *current <= 'f')) {
              current++;
              charCount++;
            } else {
              break;
            }
          }
        } else if (escaped >= '0' && escaped <= '7') {
          // Octal escape \nnn - consume up to 2 more octal digits
          for (int i = 0;
               i < 2 && current < end && *current >= '0' && *current <= '7';
               i++) {
            current++;
            charCount++;
          }
        }
        // For simple escapes like \n, \t, etc., we already consumed the
        // character
      }
      stats.characterCount += charCount;
    } else {
      ++current;
      ++stats.characterCount;
    }
  }

  if (current >= end || *current != '\'') {
    reportError(DiagnosticID::UnterminatedCharacterLiteralError,
                getLocationAt(start));
  } else {
    ++current; // consume closing quote
    ++stats.characterCount;
  }

  std::string_view text(start, current - start);
  Token token = makeToken(TokenKind::Character, start, current);

  if (hasEscape) {
    token.addFlag(TokenFlags::NeedsCleaning);
  }

  token.setText(interner.intern(text));

  ++stats.literalCount;
  return token;
}

Token Lexer::lexComment() {
  const char *start = current;

  if (current + 1 < end && current[0] == '/' && current[1] == '/') {
    // Line comment - fast scan
    current += 2;
    stats.characterCount += 2;

    const char *ptr = current;
    while (ptr < end && !isNewlineFast(*ptr)) {
      ++ptr;
    }

    stats.characterCount += (ptr - current);
    current = ptr;
    ++stats.commentCount;
    return makeToken(TokenKind::LineComment, start, current);
  } else if (current + 1 < end && current[0] == '/' && current[1] == '*') {
    // Block comment - fast scan
    current += 2;
    stats.characterCount += 2;

    const char *ptr = current;
    while (ptr + 1 < end) {
      if (ptr[0] == '*' && ptr[1] == '/') {
        ptr += 2;
        stats.characterCount += (ptr - current);
        current = ptr;
        ++stats.commentCount;
        return makeToken(TokenKind::BlockComment, start, current);
      }
      if (isNewlineFast(*ptr)) {
        stats.characterCount += (ptr - current);
        current = ptr;
        handleNewline();
        ptr = current;
      } else {
        ++ptr;
      }
    }

    // Unterminated block comment
    stats.characterCount += (ptr - current);
    current = ptr;
    ++stats.commentCount;
    return makeToken(TokenKind::BlockComment, start, current);
  }

  return makeToken(TokenKind::Unknown);
}

Token Lexer::lexOperator() {
  const char *start = current;
  unsigned char c = *current++;
  ++stats.characterCount;

  // Optimized operator parsing with reduced branching
  // Handle two-character operators first by looking ahead
  if (current < end) {
    unsigned char next = *current;
    uint16_t pair = (static_cast<uint16_t>(c) << 8) | next;

    // Fast lookup for two-character operators
    switch (pair) {
    case ('+' << 8) | '=':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::PlusEqual, 2);
    case ('+' << 8) | '+':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::PlusPlus, 2);
    case ('-' << 8) | '=':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::MinusEqual, 2);
    case ('-' << 8) | '-':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::MinusMinus, 2);
    case ('-' << 8) | '>':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::Arrow, 2);
    case ('*' << 8) | '=':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::StarEqual, 2);
    case ('/' << 8) | '=':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::SlashEqual, 2);
    case ('%' << 8) | '=':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::PercentEqual, 2);
    case ('=' << 8) | '=':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::EqualEqual, 2);
    case ('!' << 8) | '=':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::NotEqual, 2);
    case ('<' << 8) | '=':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::LessEqual, 2);
    case ('<' << 8) | '<':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::LesserLesser, 2);
    case ('>' << 8) | '=':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::GreaterEqual, 2);
    case ('>' << 8) | '>':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::GreaterGreater, 2);
    case ('&' << 8) | '&':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::AmpAmp, 2);
    case ('|' << 8) | '|':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::PipePipe, 2);
    case (':' << 8) | ':':
      ++current;
      ++stats.characterCount;
      return makeToken(TokenKind::ColonColon, 2);
    }
  }

  // Single character operators - use lookup table for better performance
  static constexpr std::array<TokenKind, 128> SINGLE_CHAR_TOKENS = []() {
    std::array<TokenKind, 128> table{};
    std::fill(table.begin(), table.end(), TokenKind::Unknown);

    table['+'] = TokenKind::Plus;
    table['-'] = TokenKind::Minus;
    table['*'] = TokenKind::Star;
    table['/'] = TokenKind::Slash;
    table['%'] = TokenKind::Percent;
    table['='] = TokenKind::Equal;
    table['!'] = TokenKind::Exclaim;
    table['<'] = TokenKind::Less;
    table['>'] = TokenKind::Greater;
    table['&'] = TokenKind::Amp;
    table['|'] = TokenKind::Pipe;
    table['^'] = TokenKind::Caret;
    table['~'] = TokenKind::Tilde;
    table['('] = TokenKind::LeftParen;
    table[')'] = TokenKind::RightParen;
    table['{'] = TokenKind::LeftBrace;
    table['}'] = TokenKind::RightBrace;
    table['['] = TokenKind::LeftBracket;
    table[']'] = TokenKind::RightBracket;
    table[';'] = TokenKind::Semicolon;
    table[','] = TokenKind::Comma;
    table['.'] = TokenKind::Dot;
    table[':'] = TokenKind::Colon;
    table['?'] = TokenKind::Question;
    table['@'] = TokenKind::At;
    table['#'] = TokenKind::Hash;
    table['\\'] = TokenKind::Backslash; // Backslash

    return table;
  }();

  if (c < 128) {
    TokenKind kind = TokenKind::Unknown;

    if (options.enableLookupTables) {
      kind = SINGLE_CHAR_TOKENS[c];
      ++stats.lookupTableHits; // Track lookup table usage
    } else {
      // Fallback for basic single character tokens without lookup table
      switch (c) {
      case '(':
        kind = TokenKind::LeftParen;
        break;
      case ')':
        kind = TokenKind::RightParen;
        break;
      case '{':
        kind = TokenKind::LeftBrace;
        break;
      case '}':
        kind = TokenKind::RightBrace;
        break;
      case '[':
        kind = TokenKind::LeftBracket;
        break;
      case ']':
        kind = TokenKind::RightBracket;
        break;
      case ';':
        kind = TokenKind::Semicolon;
        break;
      case ',':
        kind = TokenKind::Comma;
        break;
      case '.':
        kind = TokenKind::Dot;
        break;
      case ':':
        kind = TokenKind::Colon;
        break;
      case '?':
        kind = TokenKind::Question;
        break;
      case '@':
        kind = TokenKind::At;
        break;
      case '#':
        kind = TokenKind::Hash;
        break;
      case '\\':
        kind = TokenKind::Backslash;
        break;
      case '+':
        kind = TokenKind::Plus;
        break;
      case '-':
        kind = TokenKind::Minus;
        break;
      case '*':
        kind = TokenKind::Star;
        break;
      case '/':
        kind = TokenKind::Slash;
        break;
      case '%':
        kind = TokenKind::Percent;
        break;
      case '=':
        kind = TokenKind::Equal;
        break;
      case '!':
        kind = TokenKind::Exclaim;
        break;
      case '<':
        kind = TokenKind::Less;
        break;
      case '>':
        kind = TokenKind::Greater;
        break;
      case '&':
        kind = TokenKind::Amp;
        break;
      case '|':
        kind = TokenKind::Pipe;
        break;
      case '^':
        kind = TokenKind::Caret;
        break;
      case '~':
        kind = TokenKind::Tilde;
        break;
      default:
        kind = TokenKind::Unknown;
        break;
      }
    }

    if (kind != TokenKind::Unknown) {
      return makeToken(kind, 1);
    }
  }

  // Handle unknown character - provide better context
  if (c < 32 || c >= 127) {
    // Non-printable character
    diagMgr.report(DiagnosticID::UnexpectedValueError, getLocationAt(start),
                   "valid character (non-printable character)",
                   "character code: " + std::to_string(static_cast<int>(c)));
  } else {
    // Printable but unexpected character
    diagMgr.report(DiagnosticID::UnexpectedValueError, getLocationAt(start),
                   "valid character", std::string(1, c));
  }
  return makeToken(TokenKind::Unknown, 1);
}

// Utility methods
void Lexer::skipLineComment() {
  current += 2; // Skip '//'
  stats.characterCount += 2;

  // Fast scan to end of line
  const char *ptr = current;
  while (ptr < end && !isNewlineFast(*ptr)) {
    ++ptr;
  }

  stats.characterCount += (ptr - current);
  current = ptr;
}

void Lexer::skipBlockComment() {
  current += 2; // Skip '/*'
  stats.characterCount += 2;

  const char *ptr = current;
  while (ptr + 1 < end) {
    if (ptr[0] == '*' && ptr[1] == '/') {
      ptr += 2;
      stats.characterCount += (ptr - current);
      current = ptr;
      return;
    }
    if (isNewlineFast(*ptr)) {
      stats.characterCount += (ptr - current);
      current = ptr;
      handleNewline();
      ptr = current;
    } else {
      ++ptr;
    }
  }

  // If we reach here, comment was not terminated
  stats.characterCount += (ptr - current);
  current = ptr;
}

void Lexer::skipWhitespace() {
  if (options.enableSimdOptimizations) {
    // Use SIMD-optimized whitespace skipping when enabled
    auto [newPos, simdOps] = skipWhitespaceSimdWithStats(current, end);

    // Update statistics for skipped characters and SIMD operations
    stats.characterCount += (newPos - current);
    stats.simdOperations += simdOps;
    current = newPos;
  } else {
    // Fallback to simple character-by-character skipping
    const char *start = current;
    while (current < end &&
           (options.enableLookupTables ? isWhitespaceFast(*current)
                                       : isWhitespace(*current))) {
      ++current;
    }
    stats.characterCount += (current - start);
  }
}

void Lexer::handleNewline() {
  if (match('\r')) {
    match('\n'); // Handle CRLF
  } else {
    match('\n');
  }
  ++currentLine;
  lineStart = current;
}

SourceLocation Lexer::getLocationAt(const char *pos) const {
  if (!srcMgr || fid.isInvalid() || !pos) {
    return SourceLocation::getInvalidLoc();
  }

  uint32_t offset = static_cast<uint32_t>(pos - source.data());
  return srcMgr->getLocForFileOffset(fid, offset);
}

void Lexer::reportError(DiagnosticID id, SourceLocation loc) {
  diagMgr.report(id, loc);
}

void Lexer::reportError(DiagnosticID id, SourceLocation loc,
                        std::string_view arg) {
  diagMgr.report(id, loc, arg);
}

void Lexer::reportError(DiagnosticID id, SourceLocation loc,
                        std::string_view expected, std::string_view actual) {
  diagMgr.report(id, loc, expected, actual);
}

void Lexer::printStats(std::ostream &os) const {
  os << "Lexer Statistics:\n";
  os << "  Total Characters Processed: " << stats.characterCount << "\n";
  os << "  Total Tokens Lexed: " << stats.tokenCount << "\n";
  os << "  Identifiers: " << stats.identifierCount << "\n";
  os << "  Keywords: " << stats.keywordCount << "\n";
  os << "  Literals: " << stats.literalCount << "\n";
  os << "  Comments: " << stats.commentCount << "\n";
  os << "  Total Lines: " << stats.lineCount << "\n";
  os << "  Total Lexing Time (ms): " << stats.lexingTimeMs << "\n";
  os << "  SIMD Operations: " << stats.simdOperations << "\n";
  os << "  Lookup Table Hits: " << stats.lookupTableHits << "\n";
  os << "  Branch Misses: " << stats.branchMisses << "\n";
  os << "  Average Time per Token (micros): "
     << (stats.tokenCount > 0 ? (stats.lexingTimeMs * 1000.0) / stats.tokenCount
                              : 0.0)
     << "\n";
}

// TokenManager implementation
TokenManager::TokenManager(size_t initialCapacity) {
  tokens.reserve(initialCapacity);
}

TokenManager::~TokenManager() = default;

TokenManager::TokenManager(TokenManager &&other) noexcept
    : tokens(std::move(other.tokens)),
      locationIndexValid(other.locationIndexValid),
      locationIndex(std::move(other.locationIndex)) {}

TokenManager &TokenManager::operator=(TokenManager &&other) noexcept {
  if (this != &other) {
    tokens = std::move(other.tokens);
    locationIndexValid = other.locationIndexValid;
    locationIndex = std::move(other.locationIndex);
  }
  return *this;
}

void TokenManager::addToken(const Token &token) {
  tokens.push_back(token);
  locationIndexValid = false;
}

void TokenManager::addToken(Token &&token) {
  tokens.push_back(std::move(token));
  locationIndexValid = false;
}

const Token &TokenManager::getToken(size_t index) const {
  return tokens.at(index);
}

Token &TokenManager::getToken(size_t index) { return tokens.at(index); }

void TokenManager::clear() {
  tokens.clear();
  locationIndexValid = false;
  locationIndex.clear();
}

std::vector<size_t> TokenManager::findTokensInRange(SourceRange range) const {
  std::vector<size_t> result;
  result.reserve(32); // Preallocate for typical range queries

  uint32_t rangeBegin = range.getBegin().getRawEncoding();
  uint32_t rangeEnd = range.getEnd().getRawEncoding();

  // Use binary search if tokens are sorted by location and we have an index
  if (locationIndexValid && !locationIndex.empty()) {
    // Binary search optimization when index is available
    auto lower = std::lower_bound(
        locationIndex.begin(), locationIndex.end(), rangeBegin,
        [this](size_t idx, uint32_t value) {
          return tokens[idx].getLocation().getRawEncoding() < value;
        });

    auto upper = std::upper_bound(
        locationIndex.begin(), locationIndex.end(), rangeEnd,
        [this](uint32_t value, size_t idx) {
          return value < tokens[idx].getLocation().getRawEncoding();
        });

    for (auto it = lower; it != upper; ++it) {
      result.push_back(*it);
    }
  } else {
    // Linear scan with optimized comparison
    for (size_t i = 0; i < tokens.size(); ++i) {
      uint32_t loc = tokens[i].getLocation().getRawEncoding();
      if (loc >= rangeBegin && loc <= rangeEnd) {
        result.push_back(i);
      }
    }
  }

  return result;
}

size_t TokenManager::findTokenAtLocation(SourceLocation loc) const {
  for (size_t i = 0; i < tokens.size(); ++i) {
    const Token &token = tokens[i];
    SourceRange range = token.getSourceRange();

    if (loc.getRawEncoding() >= range.getBegin().getRawEncoding() &&
        loc.getRawEncoding() <= range.getEnd().getRawEncoding()) {
      return i;
    }
  }

  return SIZE_MAX; // Not found
}

std::vector<size_t> TokenManager::findTokensByKind(TokenKind kind) const {
  std::vector<size_t> result;
  result.reserve(tokens.size() / 10); // Heuristic: assume ~10% match

  // Vectorized search when possible
  const size_t vectorSize = 8;
  size_t i = 0;

  // Process tokens in chunks for better cache locality
  for (; i + vectorSize <= tokens.size(); i += vectorSize) {
    // Check 8 tokens at once
    for (size_t j = 0; j < vectorSize; ++j) {
      if (tokens[i + j].getKind() == kind) {
        result.push_back(i + j);
      }
    }
  }

  // Handle remaining tokens
  for (; i < tokens.size(); ++i) {
    if (tokens[i].getKind() == kind) {
      result.push_back(i);
    }
  }

  return result;
}

// TokenStream implementation
const Token &TokenManager::TokenStream::current() const {
  if (index >= manager.getTokenCount()) {
    static const Token eofToken(TokenKind::EndOfFile, SourceLocation(), 0);
    return eofToken;
  }
  return manager.getToken(index);
}

const Token &TokenManager::TokenStream::peek(size_t offset) const {
  size_t peekIndex = index + offset;
  if (peekIndex >= manager.getTokenCount()) {
    static const Token eofToken(TokenKind::EndOfFile, SourceLocation(), 0);
    return eofToken;
  }
  return manager.getToken(peekIndex);
}

void TokenManager::TokenStream::advance() {
  if (index < manager.getTokenCount()) {
    ++index;
  }
}

void TokenManager::printTokens(std::ostream &os) const {
  for (size_t i = 0; i < tokens.size(); ++i) {
    const Token &token = tokens[i];
    os << i << ": " << token << "\n";
  }
}

size_t TokenManager::getMemoryUsage() const {
  return tokens.size() * sizeof(Token) + locationIndex.size() * sizeof(size_t);
}

void TokenManager::buildLocationIndex() const {
  // This would build an efficient index for location-based searches
  // For now, we use linear search
  locationIndexValid = true;
}

// Convenience functions
std::vector<Token> tokenizeString(std::string_view source,
                                  StringInterner &interner,
                                  DiagnosticManager &diagMgr,
                                  const LexerOptions &opts) {
  Lexer lexer(source, interner, diagMgr, opts);
  std::vector<Token> tokens;

  // Reserve space based on source size heuristic
  // Typical token density: ~1 token per 6-8 characters
  tokens.reserve(source.size() / 7 + 64);

  Token token;
  do {
    token = lexer.nextToken();
    tokens.push_back(token);
  } while (token.getKind() != TokenKind::EndOfFile);

  return tokens;
}

std::vector<Token> tokenizeFile(const SourceManager &srcMgr, FileID fileID,
                                StringInterner &interner,
                                DiagnosticManager &diagMgr,
                                const LexerOptions &opts) {
  Lexer lexer(srcMgr, fileID, interner, diagMgr, opts);
  std::vector<Token> tokens;

  // Get file size for better token vector sizing
  const FileEntry *entry = srcMgr.getFileEntry(fileID);
  if (entry) {
    size_t fileSize = entry->getSize();
    tokens.reserve(fileSize / 7 + 64); // Heuristic: ~1 token per 7 chars
  } else {
    tokens.reserve(1024);
  }

  Token token;
  do {
    token = lexer.nextToken();
    tokens.push_back(token);
  } while (token.getKind() != TokenKind::EndOfFile);

  return tokens;
}

// BatchTokenizer implementation
BatchTokenizer::BatchTokenizer(StringInterner &interner,
                               DiagnosticManager &diagMgr,
                               const LexerOptions &opts)
    : interner(interner), diagMgr(diagMgr), options(opts) {}

std::vector<std::vector<Token>>
BatchTokenizer::tokenizeParallel(const std::vector<std::string_view> &sources) {

  std::vector<std::vector<Token>> results;
  results.reserve(sources.size());

  // For now, implement serial processing
  // TODO: Add parallel processing with thread pool
  for (const auto &source : sources) {
    auto tokens = tokenizeString(source, interner, diagMgr, options);
    results.push_back(std::move(tokens));
  }

  return results;
}

void BatchTokenizer::tokenizeStreaming(
    std::string_view source, std::function<void(const Token &)> callback) {
  Lexer lexer(source, interner, diagMgr, options);

  Token token;
  do {
    token = lexer.nextToken();
    callback(token);
  } while (token.getKind() != TokenKind::EndOfFile);

  // Update aggregate statistics
  LexerStats stats = lexer.getStats();
  aggregateStats.tokenCount += stats.tokenCount;
  aggregateStats.identifierCount += stats.identifierCount;
  aggregateStats.keywordCount += stats.keywordCount;
  aggregateStats.literalCount += stats.literalCount;
  aggregateStats.commentCount += stats.commentCount;
  aggregateStats.lineCount += stats.lineCount;
  aggregateStats.characterCount += stats.characterCount;
  aggregateStats.lexingTimeMs += stats.lexingTimeMs;
  aggregateStats.simdOperations += stats.simdOperations;
  aggregateStats.lookupTableHits += stats.lookupTableHits;
  aggregateStats.branchMisses += stats.branchMisses;
  aggregateStats.updateAverages();
}

// String processing implementation
std::string Lexer::processStringLiteral(std::string_view raw) const {
  if (raw.length() < 2)
    return std::string(raw); // Invalid string

  // Remove the quotes
  std::string_view content = raw.substr(1, raw.length() - 2);
  std::string result;
  result.reserve(content.length()); // Reserve space for efficiency

  const char *ptr = content.data();
  const char *end = ptr + content.length();

  while (ptr < end) {
    if (*ptr == '\\' && ptr + 1 < end) {
      ptr++; // Skip backslash
      char escaped = processEscapeSequence(ptr);
      result.push_back(escaped);
    } else {
      result.push_back(*ptr);
      ptr++;
    }
  }

  return result;
}

char Lexer::processCharLiteral(std::string_view raw) const {
  if (raw.length() < 3)
    return '\0'; // Invalid char literal

  // Remove the quotes
  std::string_view content = raw.substr(1, raw.length() - 2);

  if (content.empty())
    return '\0';

  if (content[0] == '\\' && content.length() >= 2) {
    const char *ptr = content.data() + 1; // Skip backslash
    return processEscapeSequence(ptr);
  } else {
    return content[0];
  }
}

char Lexer::processEscapeSequence(const char *&pos) const {
  char c = *pos++;

  switch (c) {
  case 'n':
    return '\n'; // Newline
  case 't':
    return '\t'; // Tab
  case 'r':
    return '\r'; // Carriage return
  case 'b':
    return '\b'; // Backspace
  case 'f':
    return '\f'; // Form feed
  case 'v':
    return '\v'; // Vertical tab
  case 'a':
    return '\a'; // Alert (bell)
  case '0':
    return '\0'; // Null character
  case '\\':
    return '\\'; // Backslash
  case '\'':
    return '\''; // Single quote
  case '\"':
    return '\"'; // Double quote
  case '?':
    return '\?'; // Question mark (for trigraphs)

  // Octal escape sequences (\nnn)
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7': {
    int value = c - '0';
    // Check for up to 2 more octal digits
    for (int i = 0; i < 2 && pos[0] >= '0' && pos[0] <= '7'; i++) {
      value = value * 8 + (*pos++ - '0');
    }
    return static_cast<char>(value);
  }

  // Hexadecimal escape sequences (\xnn)
  case 'x': {
    int value = 0;
    int digits = 0;

    while (digits < 2 && ((pos[0] >= '0' && pos[0] <= '9') ||
                          (pos[0] >= 'A' && pos[0] <= 'F') ||
                          (pos[0] >= 'a' && pos[0] <= 'f'))) {
      char digit = *pos++;
      if (digit >= '0' && digit <= '9') {
        value = value * 16 + (digit - '0');
      } else if (digit >= 'A' && digit <= 'F') {
        value = value * 16 + (digit - 'A' + 10);
      } else if (digit >= 'a' && digit <= 'f') {
        value = value * 16 + (digit - 'a' + 10);
      }
      digits++;
    }

    if (digits == 0) {
      // Invalid hex escape, treat as literal 'x'
      pos--; // Back up
      return 'x';
    }

    return static_cast<char>(value);
  }

  // Unicode escape sequences (\uxxxx and \Uxxxxxxxx)
  case 'u': {
    // \uxxxx - 4 hex digits for Unicode code point
    int value = 0;
    for (int i = 0; i < 4; i++) {
      if (!((pos[0] >= '0' && pos[0] <= '9') ||
            (pos[0] >= 'A' && pos[0] <= 'F') ||
            (pos[0] >= 'a' && pos[0] <= 'f'))) {
        // Invalid Unicode escape, return as is
        return c;
      }
      char digit = *pos++;
      if (digit >= '0' && digit <= '9') {
        value = value * 16 + (digit - '0');
      } else if (digit >= 'A' && digit <= 'F') {
        value = value * 16 + (digit - 'A' + 10);
      } else if (digit >= 'a' && digit <= 'f') {
        value = value * 16 + (digit - 'a' + 10);
      }
    }

    // For now, just return the lower byte (ASCII range)
    // Full Unicode support would require UTF-8 encoding
    return static_cast<char>(value & 0xFF);
  }

  case 'U': {
    // \Uxxxxxxxx - 8 hex digits for Unicode code point
    int value = 0;
    for (int i = 0; i < 8; i++) {
      if (!((pos[0] >= '0' && pos[0] <= '9') ||
            (pos[0] >= 'A' && pos[0] <= 'F') ||
            (pos[0] >= 'a' && pos[0] <= 'f'))) {
        // Invalid Unicode escape, return as is
        return c;
      }
      char digit = *pos++;
      if (digit >= '0' && digit <= '9') {
        value = value * 16 + (digit - '0');
      } else if (digit >= 'A' && digit <= 'F') {
        value = value * 16 + (digit - 'A' + 10);
      } else if (digit >= 'a' && digit <= 'f') {
        value = value * 16 + (digit - 'a' + 10);
      }
    }

    // For now, just return the lower byte (ASCII range)
    // Full Unicode support would require UTF-8 encoding
    return static_cast<char>(value & 0xFF);
  }

  default:
    // Unknown escape sequence, return the character as-is
    return c;
  }
}

} // namespace ml