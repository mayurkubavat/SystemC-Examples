add_executable(crave_rand_sim top/top.cpp)

target_include_directories(crave_rand_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/env
    ${CMAKE_CURRENT_SOURCE_DIR}/agents
    ${CMAKE_CURRENT_SOURCE_DIR}/sequences
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

# crave2uvm::crave2uvm brings in crave::crave, metaSMT, Boost and SystemC
target_link_libraries(crave_rand_sim PRIVATE
    SystemC::uvm
    crave2uvm::crave2uvm
)

add_uvm_test(crave_rand_sim)
