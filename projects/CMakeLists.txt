add_subdirectory(apb)
add_subdirectory(dma)

# Optional examples that need extra libraries. Point CMake at the installs
# with -DCRAVE_HOME=... / -DFC4SC_HOME=... (or CMAKE_PREFIX_PATH).
set(CRAVE_HOME "" CACHE PATH "Path to CRAVE installation (with crave2uvm)")
set(FC4SC_HOME "" CACHE PATH "Path to FC4SC installation")
set(SYSTEMC_LP_HOME "" CACHE PATH "Path to systemc-lp (SystemC Low Power) installation")
if(NOT SYSTEMC_LP_HOME AND DEFINED ENV{SYSTEMC_LP_HOME})
    set(SYSTEMC_LP_HOME $ENV{SYSTEMC_LP_HOME})
endif()
# CRAVE's package config also looks up its dependencies (SystemCLanguage,
# metaSMT, Boost), so put the install prefixes on the search path.
list(APPEND CMAKE_PREFIX_PATH ${CRAVE_HOME} ${FC4SC_HOME} ${SYSTEMC_LP_HOME} ${SYSTEMC_HOME})

find_package(crave2uvm CONFIG QUIET)
if(crave2uvm_FOUND)
    add_subdirectory(crave_rand)
else()
    message(STATUS "CRAVE not found — skipping projects/crave_rand. Set CRAVE_HOME to enable.")
endif()

find_package(FC4SC CONFIG QUIET)
if(FC4SC_FOUND)
    add_subdirectory(fc4sc_cov)
else()
    message(STATUS "FC4SC not found — skipping projects/fc4sc_cov. Set FC4SC_HOME to enable.")
endif()

find_package(verilator QUIET)
if(verilator_FOUND AND crave2uvm_FOUND AND FC4SC_FOUND AND
   EXISTS "${UVM_SystemC_INCLUDE_DIR}/uvmsc/reg/uvm_fc4sc_reg_cvr.h")
    add_subdirectory(verilator_apb_timer)
else()
    message(STATUS "Verilator + CRAVE + FC4SC + UVM FC4SC RAL bridge not all found — skipping projects/verilator_apb_timer.")
endif()

find_package(SystemCLP CONFIG QUIET)
if(SystemCLP_FOUND AND crave2uvm_FOUND AND FC4SC_FOUND)
    add_subdirectory(systemc_lp_apb_subsystem)
else()
    message(STATUS "systemc-lp + CRAVE + FC4SC not all found — skipping projects/systemc_lp_apb_subsystem. Set SYSTEMC_LP_HOME to enable.")
endif()
