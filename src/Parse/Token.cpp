#include "ml/Parse/Token.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <unordered_map>

namespace ml {

// Token spelling table
static const char *TokenSpellings[] = {
    "<unknown>",
    "<eof>",

    // Literals
    "<integer>",
    "<float>",
    "<string>",
    "<char>",
    "<bool>",

    // Identifiers and keywords
    "<identifier>",

    // Keywords
    "auto",
    "break",
    "case",
    "const",
    "continue",
    "default",
    "do",
    "else",
    "enum",
    "extern",
    "false",
    "for",
    "function",
    "if",
    "import",
    "let",
    "module",
    "mut",
    "null",
    "return",
    "struct",
    "switch",
    "true",
    "type",
    "var",
    "while",

    // Operators
    "+",
    "-",
    "*",
    "/",
    "%",

    // Assignment operators
    "=",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",

    // Comparison operators
    "==",
    "!=",
    "<",
    "<=",
    ">",
    ">=",

    // Logical operators
    "&&",
    "||",
    "!",

    // Bitwise operators
    "&",
    "|",
    "^",
    "~",
    "<<",
    ">>",

    // Increment/Decrement
    "++",
    "--",

    // Punctuation
    "(",
    ")",
    "{",
    "}",
    "[",
    "]",
    ";",
    ",",
    ".",
    "->",
    "::",
    ":",
    "?",
    "@",
    "#",
    "\\",

    // Comments
    "//",
    "/* */",

    // Whitespace
    "<whitespace>",
    "<newline>",
};

static_assert(sizeof(TokenSpellings) / sizeof(TokenSpellings[0]) ==
                  static_cast<size_t>(TokenKind::Count),
              "Token spellings array size mismatch");

// Token names for debugging
static const char *TokenNames[] = {
    "Unknown",
    "EndOfFile",

    // Literals
    "Integer",
    "Float",
    "String",
    "Character",
    "Boolean",

    // Identifiers and keywords
    "Identifier",

    // Keywords
    "Auto",
    "BreakKeyword",
    "Case",
    "KW_const",
    "KW_continue",
    "Default",
    "Do",
    "Else",
    "Enum",
    "Extern",
    "False",
    "For",
    "Fn",
    "If",
    "Import",
    "Let",
    "Mod",
    "Mut",
    "Null",
    "Return",
    "Struct",
    "Switch",
    "True",
    "Type",
    "Var",
    "While",

    // Operators
    "OperatorPlus",
    "OperatorMinus",
    "Star",
    "Slash",
    "Percent",

    // Assignment operators
    "Equal",
    "PlusEqual",
    "MinusEqual",
    "StarEqual",
    "SlashEqual",
    "PercentEqual",

    // Comparison operators
    "EqualEqual",
    "NotEqual",
    "Less",
    "LessEqual",
    "Greater",
    "GreaterEqual",

    // Logical operators
    "AmpAmp",
    "PipePipe",
    "Exclaim",

    // Bitwise operators
    "Amp",
    "Pipe",
    "Caret",
    "Tilde",
    "LesserLesser",
    "GreaterGreater",

    // Increment/Decrement
    "PlusPlus",
    "MinusMinus",

    // Punctuation
    "LeftParen",
    "RightParen",
    "LeftBrace",
    "RightBrace",
    "LeftBracket",
    "RightBracket",
    "Semicolon",
    "Comma",
    "Dot",
    "Arrow",
    "ColonColon",
    "Colon",
    "Question",
    "At",
    "Hash",
    "Backslash",

    // Comments
    "LineComment",
    "BlockComment",

    // Whitespace
    "Whitespace",
    "Newline",
};

static_assert(sizeof(TokenNames) / sizeof(TokenNames[0]) ==
                  static_cast<size_t>(TokenKind::Count),
              "Token names array size mismatch");

// Optimized keyword lookup using constexpr sorted array for O(log n) binary
// search
struct KeywordEntry {
  std::string_view keyword;
  TokenKind kind;

  constexpr bool operator<(const KeywordEntry &other) const {
    return keyword < other.keyword;
  }
  constexpr bool operator<(std::string_view str) const { return keyword < str; }
};

static constexpr std::array<KeywordEntry, 26> KEYWORD_LOOKUP_TABLE = {
    {{"auto", TokenKind::Auto},
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
     {"function", TokenKind::Fn},
     {"if", TokenKind::If},
     {"import", TokenKind::Import},
     {"let", TokenKind::Let},
     {"module", TokenKind::Mod},
     {"mut", TokenKind::Mut},
     {"null", TokenKind::Null},
     {"return", TokenKind::Return},
     {"struct", TokenKind::Struct},
     {"switch", TokenKind::Switch},
     {"true", TokenKind::True},
     {"type", TokenKind::Type},
     {"var", TokenKind::Var},
     {"while", TokenKind::While}}};

// Optimized operator precedence using direct array lookup
static std::array<int, static_cast<size_t>(TokenKind::Count)>
createOperatorPrecedenceTable() {
  std::array<int, static_cast<size_t>(TokenKind::Count)> table{};

  // Initialize all to 0 (not an operator)
  std::fill(table.begin(), table.end(), 0);

  // Multiplicative
  table[static_cast<size_t>(TokenKind::Star)] = 14;
  table[static_cast<size_t>(TokenKind::Slash)] = 14;
  table[static_cast<size_t>(TokenKind::Percent)] = 14;

  // Additive
  table[static_cast<size_t>(TokenKind::Plus)] = 13;
  table[static_cast<size_t>(TokenKind::Minus)] = 13;

  // Shift
  table[static_cast<size_t>(TokenKind::LesserLesser)] = 12;
  table[static_cast<size_t>(TokenKind::GreaterGreater)] = 12;

  // Relational
  table[static_cast<size_t>(TokenKind::Less)] = 11;
  table[static_cast<size_t>(TokenKind::LessEqual)] = 11;
  table[static_cast<size_t>(TokenKind::Greater)] = 11;
  table[static_cast<size_t>(TokenKind::GreaterEqual)] = 11;

  // Equality
  table[static_cast<size_t>(TokenKind::EqualEqual)] = 10;
  table[static_cast<size_t>(TokenKind::NotEqual)] = 10;

  // Bitwise AND
  table[static_cast<size_t>(TokenKind::Amp)] = 9;

  // Bitwise XOR
  table[static_cast<size_t>(TokenKind::Caret)] = 8;

  // Bitwise OR
  table[static_cast<size_t>(TokenKind::Pipe)] = 7;

  // Logical AND
  table[static_cast<size_t>(TokenKind::AmpAmp)] = 6;

  // Logical OR
  table[static_cast<size_t>(TokenKind::PipePipe)] = 5;

  // Assignment
  table[static_cast<size_t>(TokenKind::Equal)] = 2;
  table[static_cast<size_t>(TokenKind::PlusEqual)] = 2;
  table[static_cast<size_t>(TokenKind::MinusEqual)] = 2;
  table[static_cast<size_t>(TokenKind::StarEqual)] = 2;
  table[static_cast<size_t>(TokenKind::SlashEqual)] = 2;
  table[static_cast<size_t>(TokenKind::PercentEqual)] = 2;

  return table;
}

// TokenInfo implementation
const char *TokenInfo::getTokenSpelling(TokenKind kind) {
  size_t index = static_cast<size_t>(kind);
  if (index >= static_cast<size_t>(TokenKind::Count)) {
    return "<invalid>";
  }
  return TokenSpellings[index];
}

const char *TokenInfo::getTokenName(TokenKind kind) {
  size_t index = static_cast<size_t>(kind);
  if (index >= static_cast<size_t>(TokenKind::Count)) {
    return "Invalid";
  }
  return TokenNames[index];
}

bool TokenInfo::isKeyword(TokenKind kind) {
  return kind >= TokenKind::Auto && kind <= TokenKind::While;
}

TokenKind TokenInfo::getKeywordKind(std::string_view identifier) {
  // Fast binary search on sorted array
  auto it = std::lower_bound(
      KEYWORD_LOOKUP_TABLE.begin(), KEYWORD_LOOKUP_TABLE.end(), identifier,
      [](const KeywordEntry &entry, std::string_view str) {
        return entry.keyword < str;
      });

  if (it != KEYWORD_LOOKUP_TABLE.end() && it->keyword == identifier) {
    return it->kind;
  }
  return TokenKind::Identifier;
}

bool TokenInfo::isLiteral(TokenKind kind) {
  return kind >= TokenKind::Integer && kind <= TokenKind::Boolean;
}

bool TokenInfo::isOperator(TokenKind kind) {
  return kind >= TokenKind::Plus && kind <= TokenKind::MinusMinus;
}

bool TokenInfo::isPunctuation(TokenKind kind) {
  return kind >= TokenKind::LeftParen && kind <= TokenKind::Hash;
}

int TokenInfo::getOperatorPrecedence(TokenKind kind) {
  static const auto OPERATOR_PRECEDENCE = createOperatorPrecedenceTable();
  size_t index = static_cast<size_t>(kind);
  if (index >= OPERATOR_PRECEDENCE.size()) {
    return 0;
  }
  return OPERATOR_PRECEDENCE[index];
}

bool TokenInfo::isLeftAssociative(TokenKind kind) {
  // Most operators are left associative
  return getOperatorPrecedence(kind) > 0 && kind != TokenKind::Equal &&
         kind != TokenKind::PlusEqual && kind != TokenKind::MinusEqual &&
         kind != TokenKind::StarEqual && kind != TokenKind::SlashEqual &&
         kind != TokenKind::PercentEqual;
}

bool TokenInfo::isRightAssociative(TokenKind kind) {
  // Assignment operators are right associative
  return kind == TokenKind::Equal || kind == TokenKind::PlusEqual ||
         kind == TokenKind::MinusEqual || kind == TokenKind::StarEqual ||
         kind == TokenKind::SlashEqual || kind == TokenKind::PercentEqual;
}

// Stream operators
std::ostream &operator<<(std::ostream &os, TokenKind kind) {
  return os << TokenInfo::getTokenName(kind);
}

std::ostream &operator<<(std::ostream &os, const Token &token) {
  os << TokenInfo::getTokenName(token.getKind());
  if (token.getText().isValid()) {
    os << "(" << token.getText().toStringView() << ")";
  }
  return os;
}

} // namespace ml