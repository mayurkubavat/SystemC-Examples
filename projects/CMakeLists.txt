add_subdirectory(apb)
add_subdirectory(dma)

# Optional examples that need extra libraries. Point CMake at the installs
# with -DCRAVE_HOME=... / -DFC4SC_HOME=... (or CMAKE_PREFIX_PATH).
set(CRAVE_HOME "" CACHE PATH "Path to CRAVE installation (with crave2uvm)")
set(FC4SC_HOME "" CACHE PATH "Path to FC4SC installation")
# CRAVE's package config also looks up its dependencies (SystemCLanguage,
# metaSMT, Boost), so put the install prefixes on the search path.
list(APPEND CMAKE_PREFIX_PATH ${CRAVE_HOME} ${FC4SC_HOME} ${SYSTEMC_HOME})

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
