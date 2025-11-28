cmake_minimum_required(VERSION 3.20)

include(GoogleTest)

# Example test executable
add_executable(ml-tests
  exampleTest.cpp
  llvmTest.cpp
)

target_link_libraries(
  ml-tests
  PRIVATE
  GTest::gtest_main
  ${llvm_libs}
)

gtest_discover_tests(ml-tests)