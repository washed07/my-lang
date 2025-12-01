#pragma once

#include "ml/Basic/SourceLocation.hpp"
#include "ml/Basic/StringInterner.hpp"
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace ml {

/// Token types for the language
enum class TokenKind : uint16_t {
  // Special tokens
  Unknown,
  EndOfFile,

  // Literals
  Integer,
  Float,
  String,
  Character,
  Boolean,

  // Identifiers and keywords
  Identifier,

  // Keywords (alphabetical order)
  Auto,
  Break,
  Case,
  Const,
  Continue,
  Default,
  Do,
  Else,
  Enum,
  Extern,
  False,
  For,
  Fn,
  If,
  Import,
  Let,
  Mod,
  Mut,
  Null,
  Return,
  Struct,
  Switch,
  True,
  Type,
  Var,
  While,

  // Operators
  Plus,    // +
  Minus,   // -
  Star,    // *
  Slash,   // /
  Percent, // %

  // Assignment operators
  Equal,        // =
  PlusEqual,    // +=
  MinusEqual,   // -=
  StarEqual,    // *=
  SlashEqual,   // /=
  PercentEqual, // %=

  // Comparison operators
  EqualEqual,   // ==
  NotEqual,     // !=
  Less,         // <
  LessEqual,    // <=
  Greater,      // >
  GreaterEqual, // >=

  // Logical operators
  AmpAmp,   // &&
  PipePipe, // ||
  Exclaim,  // !

  // Bitwise operators
  Amp,            // &
  Pipe,           // |
  Caret,          // ^
  Tilde,          // ~
  LesserLesser,   // <<
  GreaterGreater, // >>

  // Increment/Decrement
  PlusPlus,   // ++
  MinusMinus, // --

  // Punctuation
  LeftParen,    // (
  RightParen,   // )
  LeftBrace,    // {
  RightBrace,   // }
  LeftBracket,  // [
  RightBracket, // ]
  Semicolon,    // ;
  Comma,        // ,
  Dot,          // .
  Arrow,        // ->
  ColonColon,   // ::
  Colon,        // :
  Question,     // ?
  At,           // @
  Hash,         // #
  Backslash,    // \

  // Comments
  LineComment,  // //
  BlockComment, // /* */

  // Whitespace (usually skipped)
  Whitespace,
  Newline,

  Count
};

/// Token flags for additional information
enum class TokenFlags : uint8_t {
  None = 0,
  AtStartOfLine = 1 << 0,   // Token is at the start of a line
  HasLeadingSpace = 1 << 1, // Token has leading whitespace
  NeedsCleaning = 1 << 2,   // Token text needs cleaning (escapes, etc.)
  IsKeyword = 1 << 3,       // Token is a keyword
};

inline TokenFlags operator|(TokenFlags lhs, TokenFlags rhs) {
  return static_cast<TokenFlags>(static_cast<uint8_t>(lhs) |
                                 static_cast<uint8_t>(rhs));
}

inline TokenFlags operator&(TokenFlags lhs, TokenFlags rhs) {
  return static_cast<TokenFlags>(static_cast<uint8_t>(lhs) &
                                 static_cast<uint8_t>(rhs));
}

inline TokenFlags &operator|=(TokenFlags &lhs, TokenFlags rhs) {
  return lhs = lhs | rhs;
}

/// A single token with location and metadata
class Token {
public:
  Token() : kind(TokenKind::Unknown), flags(TokenFlags::None) {}

  Token(TokenKind kind, SourceLocation loc, uint32_t length)
      : kind(kind), flags(TokenFlags::None), location(loc), length(length) {}

  Token(TokenKind kind, SourceLocation loc, uint32_t length,
        InternedString text)
      : kind(kind), flags(TokenFlags::None), location(loc), length(length),
        value(text) {}

  // Getters
  TokenKind getKind() const { return kind; }
  SourceLocation getLocation() const { return location; }
  uint32_t getLength() const { return length; }
  TokenFlags getFlags() const { return flags; }
  InternedString getText() const { return value; }

  // Setters
  void setKind(TokenKind kind) { this->kind = kind; }
  void setLocation(SourceLocation loc) { location = loc; }
  void setLength(uint32_t length) { this->length = length; }
  void setFlags(TokenFlags flags) { this->flags = flags; }
  void setText(InternedString text) { value = text; }

  // Flag operations
  bool hasFlag(TokenFlags flag) const {
    return (flags & flag) != TokenFlags::None;
  }

  void addFlag(TokenFlags flag) { flags |= flag; }

  void removeFlag(TokenFlags flag) {
    flags = static_cast<TokenFlags>(static_cast<uint8_t>(flags) &
                                    ~static_cast<uint8_t>(flag));
  }

  // Convenience methods
  bool isAtStartOfLine() const { return hasFlag(TokenFlags::AtStartOfLine); }
  bool hasLeadingSpace() const { return hasFlag(TokenFlags::HasLeadingSpace); }
  bool isKeyword() const { return hasFlag(TokenFlags::IsKeyword); }
  bool isLiteral() const {
    return kind >= TokenKind::Integer && kind <= TokenKind::Boolean;
  }
  bool isOperator() const {
    return kind >= TokenKind::Plus && kind <= TokenKind::MinusMinus;
  }
  bool isPunctuation() const {
    return kind >= TokenKind::LeftParen && kind <= TokenKind::Hash;
  }
  bool isIdentifierOrKeyword() const {
    return kind == TokenKind::Identifier ||
           (kind >= TokenKind::Auto && kind <= TokenKind::While);
  }

  // Get source range
  SourceRange getSourceRange() const {
    if (length == 0)
      return SourceRange(location);
    // This assumes we can advance source locations
    return SourceRange(location, SourceLocation::getFromRawEncoding(
                                     location.getRawEncoding() + length));
  }

  // Comparison
  bool operator==(const Token &other) const {
    return kind == other.kind && location == other.location &&
           length == other.length;
  }

  bool operator!=(const Token &other) const { return !(*this == other); }

  // Check if token matches a specific kind
  bool is(TokenKind kind) const { return this->kind == kind; }

  template <typename... Kinds>
  bool isOneOf(TokenKind kind1, Kinds... kinds) const {
    return is(kind1) || isOneOf(kinds...);
  }

  bool isOneOf() const { return false; }

  // Check if token is not a specific kind
  bool isNot(TokenKind kind) const { return this->kind != kind; }

  template <typename... Kinds> bool isNotOneOf(Kinds... kinds) const {
    return !isOneOf(kinds...);
  }

private:
  TokenKind kind;
  TokenFlags flags;
  SourceLocation location;
  uint32_t length;
  InternedString value; // For identifiers, literals, etc.
};

/// Token information and utilities
class TokenInfo {
public:
  /// Get the spelling of a token kind
  static const char *getTokenSpelling(TokenKind kind);

  /// Get the name of a token kind (for debugging)
  static const char *getTokenName(TokenKind kind);

  /// Check if a token kind is a keyword
  static bool isKeyword(TokenKind kind);

  /// Get keyword token kind from identifier text
  static TokenKind getKeywordKind(std::string_view identifier);

  /// Check if a token kind is a literal
  static bool isLiteral(TokenKind kind);

  /// Check if a token kind is an operator
  static bool isOperator(TokenKind kind);

  /// Check if a token kind is punctuation
  static bool isPunctuation(TokenKind kind);

  /// Get operator precedence (0 = not an operator)
  static int getOperatorPrecedence(TokenKind kind);

  /// Check if operator is left associative
  static bool isLeftAssociative(TokenKind kind);

  /// Check if operator is right associative
  static bool isRightAssociative(TokenKind kind);
};

/// Stream output for tokens
std::ostream &operator<<(std::ostream &os, TokenKind kind);
std::ostream &operator<<(std::ostream &os, const Token &token);

} // namespace ml