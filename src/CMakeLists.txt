cmake_minimum_required(VERSION 3.20)

add_executable(my-lang
    main.cpp
)

# Link LLVM libraries to your executable
target_link_libraries(my-lang PRIVATE ${llvm_libs})