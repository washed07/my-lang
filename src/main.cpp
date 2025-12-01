#include "ml/Basic/ArenaAllocator.hpp"
#include "ml/Basic/StringInterner.hpp"
#include "ml/Managers/DiagnosticManager.hpp"
#include "ml/Managers/FileManager.hpp"
#include "ml/Managers/SourceManager.hpp"
#include "ml/Parse/Lexer.hpp"
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  // Create arena allocator for efficient memory management
  ml::ArenaAllocator arena(2 * 1024 * 1024); // 12MB chunks

  // Use arena-backed string interner for better memory locality
  ml::StringInterner interner(arena);
  ml::FileManager fileMgr(interner);
  auto file = fileMgr.getFile(argv[1]);
  ml::SourceManager srcMgr(fileMgr);
  ml::DiagnosticManager diagMgr(interner);
  diagMgr.setSourceManager(
      &srcMgr); // Set the SourceManager for proper location reporting
  diagMgr.addConsumer(std::make_unique<ml::TextDiagnosticConsumer>(std::cout));

  // Configure lexer options with SIMD disabled
  ml::LexerOptions opts;
  opts.enableSimdOptimizations = false;
  opts.enableLookupTables = false; // Keep lookup tables enabled
  opts.enableFastPath = false;     // Keep fast path enabled

  ml::Lexer lexer(srcMgr, srcMgr.createFileID(file->getFilenameView().data()),
                  interner, diagMgr, opts);
  std::vector<ml::Token> tokens;
  while (!lexer.isAtEnd()) {
    ml::Token token = lexer.nextToken();
    tokens.push_back(token);
  }
  lexer.printStats(std::cout);
  srcMgr.printStats();
  interner.printStats(std::cout);
  diagMgr.printStats(std::cout);

  std::cout << "\n";
  arena.printStats(std::cout);
  return 0;
}
