# Contributing to My Language

Thank you for your interest in contributing to **My Language**! This document provides comprehensive guidelines for contributing to the project.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [How to Contribute](#how-to-contribute)
3. [Development Workflow](#development-workflow)
4. [Code Style and Standards](#code-style-and-standards)
5. [Naming Conventions](#naming-conventions)
6. [Code Organization](#code-organization)
7. [Comments and Documentation](#comments-and-documentation)
8. [Testing Guidelines](#testing-guidelines)
9. [Pull Request Process](#pull-request-process)
10. [Community Guidelines](#community-guidelines)

---

## Getting Started

### Prerequisites

Before contributing, ensure you have:

- **C++20** compatible compiler (GCC 10+, Clang 11+, or MSVC 2019+)
- **CMake 3.20** or higher
- **Git** for version control
- **clang-tidy** and **clang-format** installed for code formatting
- **GoogleTest** (automatically fetched by CMake)

### Setting Up Your Development Environment

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/my_lang.git
   cd my_lang
   ```

3. **Add the upstream remote**:
   ```bash
   git remote add upstream https://github.com/washed07/my_lang.git
   ```

4. **Create a build directory**:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

5. **Configure your editor** to use the provided `.clang-format` and `.clang-tidy` files

### Building the Project

```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Run tests
make test
# or
ctest --output-on-failure
```

---

## How to Contribute

We welcome various types of contributions:

### 🐛 Bug Reports

Found a bug? Please [open an issue](https://github.com/washed07/my_lang/issues) with:
- Clear, descriptive title
- Steps to reproduce the bug
- Expected vs actual behavior
- Your environment (OS, compiler version, etc.)
- Code samples or error messages

### ✨ Feature Requests

Have an idea? [Open an issue](https://github.com/washed07/my_lang/issues) describing:
- The problem you're trying to solve
- Your proposed solution
- Any alternatives you've considered
- Why this feature would be useful to others

### 📝 Documentation

Documentation improvements are always welcome:
- Fix typos or clarify existing docs
- Add examples or tutorials
- Improve code comments
- Translate documentation

### 💻 Code Contributions

Ready to code? Great! Follow the [Development Workflow](#development-workflow) below.

---

## Development Workflow

### 1. Create a Feature Branch

Always work on a feature branch, never on `main`:

```bash
# Fetch the latest changes
git fetch upstream
git checkout main
git merge upstream/main

# Create your feature branch
git checkout -b feature/amazing-feature
# or
git checkout -b bugfix/issue-123
```

**Branch naming conventions:**
- `feature/` - New features
- `bugfix/` - Bug fixes
- `docs/` - Documentation updates
- `refactor/` - Code refactoring
- `test/` - Test additions or fixes

### 2. Make Your Changes

- Write clean, well-documented code
- Follow all style guidelines (see below)
- Add tests for new functionality
- Update documentation as needed

### 3. Test Your Changes

```bash
# Build the project
cd build
make

# Run all tests
make test

# Run clang-tidy
clang-tidy path/to/File.cpp

# Run clang-format (check mode)
clang-format --dry-run --Werror path/to/File.cpp

# Auto-format (if needed)
clang-format -i path/to/File.cpp
```

### 4. Commit Your Changes

Write clear, descriptive commit messages:

```bash
git add .
git commit -m "Add feature: brief description

- More detailed explanation of what changed
- Why the change was necessary
- Any breaking changes or important notes"
```

**Commit message guidelines:**
- Use present tense ("Add feature" not "Added feature")
- First line should be 50 characters or less
- Separate subject from body with blank line
- Reference issues: "Fixes #123" or "Closes #456"

### 5. Push and Create Pull Request

```bash
git push origin feature/amazing-feature
```

Then open a Pull Request on GitHub with:
- Clear description of changes
- Reference to related issues
- Screenshots (if UI changes)
- Notes on testing performed

---

## Code Style and Standards

**All contributions must strictly adhere to these standards** to maintain code consistency and readability.

The project uses **clang-tidy with the LLVM configuration** for automated style enforcement. All code must pass clang-tidy checks before being merged.

### Core Principles

- **Readability First**: Code is read more often than written
- **Consistency**: Follow existing patterns in the codebase
- **Simplicity**: Prefer simple, clear solutions over clever ones
- **Documentation**: Document the "why", not the "what"
- **LLVM Conventions**: Follow LLVM coding standards where applicable

---

## Naming Conventions

### Convention Definitions

The following naming patterns are used throughout the project, following LLVM conventions where applicable:

| Convention | Format | Example |
|:-----------|:-------|:--------|
| **PascalCase** | First letter of every word capitalized | `MyExample` |
| **xPascalCase** | Prefixed with 'x', then PascalCase | `xMyExample` |
| **kPascalCase** | Prefixed with 'k', then PascalCase | `kMyExample` |
| **gPascalCase** | Prefixed with 'g', then PascalCase | `gMyExample` |
| **sPascalCase** | Prefixed with 's', then PascalCase | `sMyExample` |
| **uPascalCase** | Prefixed with 'u', then PascalCase | `uMyExample` |
| **ePascalCase** | Prefixed with 'e', then PascalCase | `eMyExample` |
| **camelCase** | First word lowercase, subsequent words capitalized | `myExample` |
| **camelCase_** | camelCase with trailing underscore | `myExample_` |
| **ALL_UPPER** | All uppercase with underscore separators | `MY_EXAMPLE` |

### Naming Reference Table

| Item | Convention | Example |
|:-----|:-----------|:--------|
| Local Variable | camelCase | `tokenCount` | 
| Global Variable | gPascalCase | `gAppSettings` | 
| Static Variable | sPascalCase | `sInstanceCount` | 
| Function | camelCase | `parseExpression()` | 
| Constant | kPascalCase | `kMaxTokens` | 
| Global Constant | kPascalCase | `kDefaultTimeout` |
| Static Constant | kPascalCase | `kBufferSize` |
| Constexpr Variable | kPascalCase | `kPi` |
| Constexpr Function | camelCase | `square()` | 
| Type / Class | PascalCase | `SyntaxTree` |
| Union | uPascalCase | `uValue` |
| Enum Type | ePascalCase | `eTokenType` |
| Enum Variant | PascalCase | `Identifier` | 
| Scoped Enum Variant | PascalCase | `Red` | 
| Preprocessor Macro | ALL_UPPER | `DEBUG_MODE` |
| Public Member | PascalCase | `NodeType` | 
| Protected Member | PascalCase | `BaseValue` | 
| Private Member | camelCase_ | `internalState_` | 
| Constant Member | kPascalCase | `kDefaultValue` | 
| Static Member | sPascalCase | `sInstanceCount` | 
| Template Parameter | PascalCase | `typename T` | 
| Namespace | snake_case | `ast_utils` | 
| File Name | PascalCase | `TokenParser.cpp` |

**Important Note:** The table above reflects our project's custom naming scheme. While we use LLVM's clang-tidy configuration for enforcement, we have extended and customized certain conventions:
- **Private members** use `camelCase_` with trailing underscore (LLVM style)
- **Global variables** use `gPascalCase` prefix (project extension)
- **Static variables** use `sPascalCase` prefix (project extension)
- **Static members** use `sPascalCase` prefix (project extension)

### Variables and Functions

#### General Principles

- **Use descriptive, meaningful names.** Names should clearly convey purpose and intent without requiring additional context. Avoid cryptic abbreviations unless universally recognized.
  
  ```cpp
  // Good
  int syntaxErrorCount;
  Token nextToken;
  
  // Bad
  int sec;  // What does 'sec' mean?
  Token nt;
  ```

- **Avoid single-letter names** except for:
  - Loop counters (`i`, `j`, `k`)
  - Common mathematical variables (`x`, `y`, `z`)
  - Template parameters (`T`, `U`)
  - Lambda parameters in trivial cases

- **Use standard abbreviations sparingly** and only when widely recognized:
  - `IR` for Intermediate Representation
  - `AST` for Abstract Syntax Tree
  - `Ctx` for context
  - `Ptr` for pointer
  - `Src` for source, `Dst` for destination
  - `Decl` for declaration
  - `Expr` for expression
  - `Stmt` for statement

#### Variable Scope Conventions

The project uses prefixes to distinguish variable scope:

```cpp
// Local variables - camelCase
void processToken() {
  int tokenCount = 0;
  bool isValid = true;
}

// Global variables - gPascalCase
gPascalCase gApplicationConfig;
int gTotalErrors = 0;

// Static variables (file scope) - sPascalCase
static int sInstanceCount = 0;
static bool sIsInitialized = false;

// Constants (any scope) - kPascalCase
const int kMaxTokens = 1000;
constexpr double kPi = 3.14159;
static const size_t kBufferSize = 4096;
```

**Rationale:** These prefixes make it immediately clear when code is accessing global state or static storage, which helps prevent bugs and makes code review easier.

#### Class Member Conventions

```cpp
class Parser {
public:
  // Public members - PascalCase
  int ErrorCount;
  std::string FileName;
  
  // Constant members - kPascalCase
  static const int kMaxDepth = 100;
  
  // Static members - sPascalCase
  static int sParserCount;
  
protected:
  // Protected members - PascalCase
  Token CurrentToken;
  
private:
  // Private members - camelCase_
  int depth_;
  bool isInitialized_;
  std::vector<Token> tokenBuffer_;
  
  // Private static members - sPascalCase
  static bool sDebugMode;
};
```

#### Getters and Setters

Follow LLVM conventions for accessor methods:

```cpp
// Good - LLVM style (no 'get' prefix for simple accessors)
int lineNumber() const;
void setLineNumber(int Line);

// Use 'get' prefix when the operation is non-trivial
Type *getType() const;
Module *getModule() const;

// Boolean properties use 'is' or 'has' prefix
bool isValid() const;
bool hasErrors() const;
```

#### Function Naming

- **Use verb phrases** for functions that perform actions: `parseToken()`, `generateCode()`, `validateSyntax()`
- **Use predicate naming** for boolean-returning functions: `isValid()`, `hasChildren()`, `canOptimize()`
- **Parameter names in declarations** should be PascalCase to match LLVM style:

```cpp
void processNode(SyntaxNode *Node, Context &Ctx);
bool validateToken(const Token &Tok, int LineNumber);
```

- **Be consistent with verb choice:**
  - `create` / `destroy` for resource management
  - `begin` / `end` for iteration
  - `open` / `close` for file operations
  - `start` / `stop` for processes

#### Boolean Variables

Prefix boolean variables with interrogatives to make intent clear:

```cpp
bool isInitialized;
bool hasErrors;
bool canContinue;
bool shouldOptimize;
```

### Types and Data Structures

#### Type Definitions

- **Use PascalCase** for all user-defined types (LLVM style)
- **Prefer descriptive names** over generic ones
- **Include context** when necessary to avoid ambiguity

```cpp
// Good
class SyntaxTree;
struct TokenMetadata;
class CompilationUnit;

// Bad
class Tree;  // Too generic
struct Data;
```

#### Enumerations

- **Enum types** use `ePascalCase` (project convention)
- **Enum values** use `PascalCase` (LLVM style)
- **Use scoped enums** (enum class) when possible
- **Scoped enum constants** also use `PascalCase`

```cpp
enum class eTokenType {
  Identifier,
  Keyword,
  Operator,
  Literal,
  EndOfFile
};

enum class eOptimizationLevel {
  None,
  Basic,
  Aggressive
};

// Both scoped and unscoped enum constants use PascalCase
enum eColor {
  Red,
  Green,
  Blue
};
```

#### Unions and Variants

Tagged unions use `uPascalCase` (project convention):

```cpp
union uLiteralValue {
  int64_t IntValue;
  double FloatValue;
  const char *StringValue;
};
```

#### Template Parameters

Use PascalCase with descriptive names when clarity is needed:

```cpp
template <typename NodeType, typename VisitorFunc>
void traverseTree(NodeType *Root, VisitorFunc Visitor);

// Simple cases can use single letters
template <typename T>
class Container;

// Template template parameters also use PascalCase
template <template <typename> class Container, typename T>
class Adapter;
```

---

## Code Organization

### File Structure

- **One primary class per file** (exceptions for tightly coupled helper classes)
- **Header files** use `.h` extension
- **Implementation files** use `.cpp` extension
- **File names** use PascalCase matching the primary class name (LLVM style)

```
SyntaxTree.h
SyntaxTree.cpp
TokenParser.h
TokenParser.cpp
```

### Header Guards

Use `#pragma once` or include guards following LLVM conventions:

```cpp
#ifndef MYLANG_PARSER_SYNTAXTREE_H
#define MYLANG_PARSER_SYNTAXTREE_H

// ... content ...

#endif // MYLANG_PARSER_SYNTAXTREE_H
```

### Include Order

Organize includes in the following order (LLVM style), separated by blank lines:

1. Main Module Header (for .cpp files)
2. Local/Private Headers
3. Project headers
4. System libraries (llvm/*, clang/*)
5. System headers

```cpp
#include "SyntaxTree.h"

#include "Token.h"
#include "Parser.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Casting.h"

#include <memory>
#include <vector>
```

### Namespace Usage

- Use snake_case for namespace names (LLVM style)
- **Inline namespaces** also use snake_case
- Never use `using namespace` in headers
- Avoid `using namespace std` in general
- Prefer explicit qualification or specific `using` declarations

```cpp
namespace my_language {
namespace ast {

class SyntaxNode {
  // ...
};

} // namespace ast
} // namespace my_language

// Inline namespaces for versioning
namespace my_language {
inline namespace v2 {
  // New API
}
} // namespace my_language
```

Use anonymous namespaces for internal linkage in .cpp files:

```cpp
namespace {

// Internal helper function
void helperFunction() {
  // ...
}

} // anonymous namespace
```

---

## Comments and Documentation

### Documentation Comments

Use Doxygen-style comments for all public APIs:

```cpp
/// Parses a token stream into an abstract syntax tree.
///
/// \param Tokens The input token stream
/// \param Ctx The compilation context
/// \return A pointer to the root AST node, or nullptr on error
/// \throws ParseException if syntax errors are encountered
SyntaxNode *parseTokens(const TokenStream &Tokens, Context &Ctx);
```

LLVM prefers `///` for doc comments and `\param` style markers.

### Inline Comments

- **Explain why, not what.** Code should be self-documenting for basic operations
- Place comments on the line above the code they describe
- Use `//` for single-line comments
- Keep comments concise and relevant

```cpp
// Check for overflow before allocation
if (Size > kMaxAllocationSize)
  return nullptr;

// TODO: Optimize this loop for large inputs
for (const auto &Tok : Tokens)
  processToken(Tok);
```

### Comment Tags

Use standardized tags for special comments:

- `TODO:` for planned improvements
- `FIXME:` for known issues requiring attention
- `NOTE:` for important clarifications
- `HACK:` for temporary workarounds

---

## Testing Guidelines

### Writing Tests

- **Every new feature** must include tests
- **Every bug fix** should include a test that would have caught the bug
- Use GoogleTest framework
- Place tests in `tests/` directory
- Name test files `Test<ComponentName>.cpp`

### Test Structure

```cpp
#include <gtest/gtest.h>
#include "YourComponent.h"

TEST(ComponentNameTest, DescriptiveTestName) {
  // Arrange
  YourComponent component;
  
  // Act
  auto result = component.doSomething();
  
  // Assert
  EXPECT_EQ(result, expectedValue);
}

TEST(ComponentNameTest, EdgeCaseHandling) {
  YourComponent component;
  
  EXPECT_THROW(component.invalidOperation(), std::exception);
}
```

### Test Coverage

Aim for comprehensive test coverage:
- **Happy path** - Normal, expected usage
- **Edge cases** - Boundary conditions
- **Error conditions** - Invalid inputs
- **Integration** - Components working together

### Running Tests

```bash
# Run all tests
make test

# Run specific test
./build/tests/TestComponentName

# Run with verbose output
ctest --output-on-failure --verbose
```

---

## Formatting and Style

### Indentation and Spacing

- **Use 2 spaces** for indentation (LLVM standard)
- **No tabs** - configure your editor to use spaces
- **Maximum line length:** 80 characters (LLVM standard)
- **Blank lines:**
  - One blank line between function definitions
  - One blank line between logical sections within functions

### Braces

Follow LLVM brace style:

```cpp
// Attach opening brace for functions and classes
void function() {
  doSomething();
}

class MyClass {
public:
  void method() {
    // implementation
  }
};

// For control structures, omit braces for single statements
if (Condition)
  doSomething();
else
  doSomethingElse();

// Use braces for multi-line or complex single statements
if (ComplexCondition) {
  // Complex single statement that spans
  // multiple lines
  doComplexThing();
}
```

### Spacing

```cpp
// Operators - spaces around binary operators
int Result = A + B * C;
bool Check = (X > 5) && (Y < 10);

// No space before function call parentheses
parseToken(Token, Context);

// Control structures - no space before parentheses
if (Condition) {
for (int I = 0; I < Count; ++I) {
while (IsRunning) {
```

### Pointer and Reference Declarations

Attach `*` and `&` to the **variable name**, not the type (LLVM style):

```cpp
int *Ptr;
const std::string &Name;
void processNode(SyntaxNode *Node);

// For multiple declarations, one per line
int *Ptr1;
int *Ptr2;
```

### Best Practices

#### Const Correctness

- Use `const` liberally for variables, parameters, and member functions
- Prefer `const` references for parameters that won't be modified
- Follow LLVM style: `const` goes after the type for pointers

```cpp
void analyzeTree(const SyntaxTree &Tree);
const Token &getCurrentToken() const;
SyntaxNode *const getRoot() const; // const pointer
```

#### Memory Management

- Prefer LLVM ADT containers (`SmallVector`, `DenseMap`, etc.)
- Use smart pointers (`std::unique_ptr`) when ownership is clear
- Document ownership semantics clearly
- Prefer stack allocation over heap when possible

```cpp
std::unique_ptr<SyntaxNode> parseExpression();
llvm::SmallVector<Token, 16> Tokens; // Inline storage for 16 elements
```

#### Error Handling

- Use `llvm::Error` and `llvm::Expected<T>` for recoverable errors
- Use assertions for invariants and programmer errors
- Document error conditions in function documentation

```cpp
// Returns Error on failure
llvm::Error validateSyntax(const SyntaxTree &Tree);

// Returns Expected<T> with value or error
llvm::Expected<Token> parseToken(const std::string &Input);
```

#### Magic Numbers

Avoid magic numbers; use named constants:

```cpp
// Bad
if (Buffer.size() > 1024)

// Good
static constexpr size_t kMaxBufferSize = 1024;
if (Buffer.size() > kMaxBufferSize)
```

### LLVM-Specific Guidelines

#### Using LLVM ADT

Prefer LLVM's ADT (Abstract Data Types) over STL equivalents when appropriate:

```cpp
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

// SmallVector avoids heap allocation for small sizes
llvm::SmallVector<Token, 8> Tokens;

// DenseMap is more efficient than std::unordered_map for pointer keys
llvm::DenseMap<Node *, Value> NodeToValue;

// StringRef avoids unnecessary string copies
void processName(llvm::StringRef Name);
```

#### LLVM Casting

Use LLVM's casting utilities instead of C++ casts:

```cpp
#include "llvm/Support/Casting.h"

// Instead of dynamic_cast
if (auto *Expr = llvm::dyn_cast<ExprNode>(Node)) {
  // Use Expr
}

// For guaranteed casts
ExprNode *Expr = llvm::cast<ExprNode>(Node);

// For checking type
if (llvm::isa<ExprNode>(Node)) {
  // Node is an ExprNode
}
```

---

## Pull Request Process

### Before Submitting

Ensure your PR:
- [ ] Follows all code style guidelines
- [ ] Passes all existing tests
- [ ] Includes tests for new functionality
- [ ] Updates documentation as needed
- [ ] Has descriptive commit messages
- [ ] Passes clang-tidy checks
- [ ] Passes clang-format checks

### PR Checklist

```bash
# Run full test suite
make test

# Run clang-tidy
run-clang-tidy -p build/

# Check formatting
find src -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror

# Auto-fix formatting if needed
find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### PR Description Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
Describe how you tested your changes

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-review completed
- [ ] Comments added for complex code
- [ ] Documentation updated
- [ ] Tests added/updated
- [ ] All tests pass
- [ ] clang-tidy passes
- [ ] clang-format passes

## Related Issues
Fixes #(issue number)
```

### Review Process

1. **Automated checks** will run on your PR
2. **Maintainers** will review your code
3. **Address feedback** by pushing new commits
4. Once approved, your PR will be **merged**

---

## Community Guidelines

### Code of Conduct

- **Be respectful** and constructive
- **Welcome newcomers** and help them learn
- **Focus on the code**, not the person
- **Assume good intentions**
- **Ask questions** if something is unclear

### Getting Help

- **Check existing issues** before creating new ones
- **Read documentation** thoroughly
- **Ask in discussions** for general questions
- **Be patient** - maintainers are volunteers

### Recognition

Contributors will be:
- Listed in the project's contributors page
- Acknowledged in release notes for significant contributions
- Welcomed as part of the community

---

## Enforcement

All code contributions will be:
1. **Automatically checked** by clang-tidy in CI/CD
2. **Manually reviewed** for adherence to project-specific conventions
3. **Rejected** if they fail clang-tidy checks or violate these guidelines

Configure your editor to run clang-tidy on save to catch issues early.

---

## Questions?

If you have questions about contributing, please:
- Open a [discussion](https://github.com/washed07/my_lang/discussions)
- Contact the maintainers
- Check existing documentation

---

**Thank you for contributing to My Language!** Your efforts help make this educational resource better for everyone learning about compiler design and implementation.

---

<div align="center">

**[⭐ Star this repo](https://github.com/washed07/my_lang)** if you found it helpful!

Made with ❤️ for the programming language community

</div>