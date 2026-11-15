add_executable(fc4sc_cov_sim top/top.cpp)

target_include_directories(fc4sc_cov_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/env
    ${CMAKE_CURRENT_SOURCE_DIR}/agents
    ${CMAKE_CURRENT_SOURCE_DIR}/sequences
    ${CMAKE_CURRENT_SOURCE_DIR}/test
)

# FC4SC is header-only
target_link_libraries(fc4sc_cov_sim PRIVATE
    SystemC::uvm
    FC4SC::fc4sc
)
