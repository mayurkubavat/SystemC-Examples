// ----------------------------------------------------------------------------
// top.cpp — sc_main for the FC4SC functional coverage example
//
// Usage:  ./fc4sc_cov_sim [seed]
// Writes fc4sc_cov_results.xml (UCIS XML) to the working directory.
// ----------------------------------------------------------------------------
#include <systemc>
#include <uvm>

#include <cstdlib>

#include "bus_cov_test.h"

int sc_main(int argc, char* argv[]) {
  unsigned seed = 1;
  if (argc > 1) seed = static_cast<unsigned>(std::strtoul(argv[1], nullptr, 0));
  uvm::uvm_config_db<unsigned>::set(nullptr, "*", "seed", seed);

  uvm::run_test("bus_cov_test");

  uvm::uvm_report_server* svr = uvm::uvm_report_server::get_server();
  return (svr->get_severity_count(uvm::UVM_ERROR) + svr->get_severity_count(uvm::UVM_FATAL)) ? 1 : 0;
}
