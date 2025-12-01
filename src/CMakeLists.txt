cmake_minimum_required(VERSION 3.20)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)

add_executable(my-lang
  ${SOURCE_DIR}/main.cpp
  ${SOURCE_DIR}/Basic/ArenaAllocator.cpp
  ${SOURCE_DIR}/Basic/StringInterner.cpp
  ${SOURCE_DIR}/Managers/DiagnosticManager.cpp
  ${SOURCE_DIR}/Managers/FileManager.cpp
  ${SOURCE_DIR}/Managers/SourceManager.cpp
  ${SOURCE_DIR}/Parse/Lexer.cpp
  ${SOURCE_DIR}/Parse/Token.cpp
)

target_include_directories(my-lang PRIVATE
  ${INCLUDE_DIR}
)

# Link LLVM libraries to your executable
target_link_libraries(my-lang PRIVATE ${llvm_libs})
