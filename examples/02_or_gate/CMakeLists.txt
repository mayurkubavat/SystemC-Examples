add_executable(or_gate or_gate.cpp)
target_link_libraries(or_gate SystemC::systemc)
add_sim_test(or_gate)
