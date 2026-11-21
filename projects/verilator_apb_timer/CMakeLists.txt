# Full-stack Verilator RTL + UVM-SystemC + CRAVE + FC4SC verification example.
# Builds both the clean RTL target (verilator_apb_timer_sim) and optional
# fault-injected targets (BUG_SEED=1..3) for regression & bug-detection evaluation.

function(add_apb_timer_variant target_name bug_seed)
    add_executable(${target_name} top/top.cpp)

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/rtl
        ${CMAKE_CURRENT_SOURCE_DIR}/env
        ${CMAKE_CURRENT_SOURCE_DIR}/agents
        ${CMAKE_CURRENT_SOURCE_DIR}/sequences
        ${CMAKE_CURRENT_SOURCE_DIR}/test
    )

    target_compile_definitions(${target_name} PRIVATE BUG_SEED=${bug_seed})

    target_link_libraries(${target_name} PRIVATE
        SystemC::systemc
        SystemC::uvm
        crave2uvm::crave2uvm
        FC4SC::fc4sc
    )

    verilate(${target_name}
        SYSTEMC
        PREFIX Vapb_timer
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/rtl/apb_timer.sv
        VERILATOR_ARGS -DBUG_SEED=${bug_seed}
    )
endfunction()

add_apb_timer_variant(verilator_apb_timer_sim 0)
add_uvm_test(verilator_apb_timer_sim)

option(BUILD_APB_TIMER_BUG_VARIANTS "Build fault-injected RTL variants (BUG_SEED=1..3)" ON)
if(BUILD_APB_TIMER_BUG_VARIANTS)
    add_apb_timer_variant(verilator_apb_timer_bug1 1)
    add_apb_timer_variant(verilator_apb_timer_bug2 2)
    add_apb_timer_variant(verilator_apb_timer_bug3 3)
endif()
