cmake_minimum_required(VERSION 3.20)

add_executable(ml
    main.cpp
)

# Link LLVM libraries to your executable
target_link_libraries(ml PRIVATE ${llvm_libs})