cmake_minimum_required(VERSION 3.14)
project(SystemC_Learning LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SYSTEMC_HOME "" CACHE PATH "Path to SystemC installation")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(SystemC REQUIRED)

# Regression: every example and testbench is a ctest test.
#   ctest --test-dir build --output-on-failure
enable_testing()

# add_sim_test(<target>): run a simulation in its build directory. The test
# fails on a non-zero exit code.
function(add_sim_test target)
    add_test(NAME ${target} COMMAND ${target}
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

# add_uvm_test(<target>): as add_sim_test, and the test also fails when the
# UVM report summary counts a UVM_ERROR or UVM_FATAL.
function(add_uvm_test target)
    add_sim_test(${target})
    set_tests_properties(${target} PROPERTIES
        FAIL_REGULAR_EXPRESSION "UVM_ERROR +: +[1-9];UVM_FATAL +: +[1-9]")
endfunction()

add_subdirectory(examples)

if(UVM_SystemC_FOUND)
    add_subdirectory(projects)
else()
    message(STATUS "UVM-SystemC not found — skipping projects/. Set UVM_SYSTEMC_HOME to enable.")
endif()
