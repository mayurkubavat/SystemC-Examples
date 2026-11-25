function(add_systemc_lp_subsys_target target_name lp_lib lp_bug)
    add_executable(${target_name} top/top.cpp)

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/rtl
        ${CMAKE_CURRENT_SOURCE_DIR}/env
        ${CMAKE_CURRENT_SOURCE_DIR}/test
    )

    target_compile_definitions(${target_name} PRIVATE
        SC_INCLUDE_DYNAMIC_PROCESSES
        LP_BUG=${lp_bug}
    )

    target_link_libraries(${target_name} PRIVATE
        SystemC::systemc
        SystemC::uvm
        crave2uvm::crave2uvm
        FC4SC::fc4sc
        ${lp_lib}
    )
endfunction()

# 1. Main binary linked with SystemC::lp (supports both LP-OFF and --upf LP-ON!)
add_systemc_lp_subsys_target(systemc_lp_apb_subsys_sim SystemC::lp 0)

# 2. Link-swapped binary linked with SystemC::lp_stub (zero Tcl, LP compiled in but link-stubbed)
add_systemc_lp_subsys_target(systemc_lp_apb_subsys_stub_sim SystemC::lp_stub 0)

# Positive regression CTests (3 modes: Functional CLI-off, Full UPF CLI-on, Link-time Stub)
add_test(NAME systemc_lp_apb_subsys_func
    COMMAND systemc_lp_apb_subsys_sim
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(systemc_lp_apb_subsys_func PROPERTIES
    FAIL_REGULAR_EXPRESSION "UVM_ERROR +: +[1-9];UVM_FATAL +: +[1-9]")

add_test(NAME systemc_lp_apb_subsys_upf
    COMMAND systemc_lp_apb_subsys_sim
        --upf "${CMAKE_CURRENT_SOURCE_DIR}/rtl/apb_subsys.upf"
        --lp-ucis "${CMAKE_CURRENT_BINARY_DIR}/systemc_lp_apb_subsys_ucis.xml"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(systemc_lp_apb_subsys_upf PROPERTIES
    FAIL_REGULAR_EXPRESSION "UVM_ERROR +: +[1-9];UVM_FATAL +: +[1-9]")

add_test(NAME systemc_lp_apb_subsys_stub
    COMMAND systemc_lp_apb_subsys_stub_sim
        --upf "${CMAKE_CURRENT_SOURCE_DIR}/rtl/apb_subsys.upf"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(systemc_lp_apb_subsys_stub PROPERTIES
    FAIL_REGULAR_EXPRESSION "UVM_ERROR +: +[1-9];UVM_FATAL +: +[1-9]")

# Fault-injected low-power bug targets (LP_BUG=1..3: must fail deterministically under --upf!)
option(BUILD_SYSTEMC_LP_SUBSYS_BUG_VARIANTS "Build low-power fault-injection variants (LP_BUG=1..3)" ON)
if(BUILD_SYSTEMC_LP_SUBSYS_BUG_VARIANTS)
    add_systemc_lp_subsys_target(systemc_lp_apb_subsys_bug1 SystemC::lp 1)
    add_systemc_lp_subsys_target(systemc_lp_apb_subsys_bug2 SystemC::lp 2)
    add_systemc_lp_subsys_target(systemc_lp_apb_subsys_bug3 SystemC::lp 3)
endif()
