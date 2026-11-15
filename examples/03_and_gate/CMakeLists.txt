add_executable(and_gate and_gate.cpp)
target_link_libraries(and_gate SystemC::systemc)
add_sim_test(and_gate)
