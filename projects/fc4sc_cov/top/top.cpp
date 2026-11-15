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
  // fc4sc_cov_sim [seed] [num_items] [goal]
  // A short run with goal 0 is one seed of a coverage regression, whose
  // results are merged afterwards (see scripts/cov_regress.sh).
  unsigned seed = 1;
  unsigned num_items = 300;
  double goal = 100.0;  // percent
  if (argc > 1) seed = static_cast<unsigned>(std::strtoul(argv[1], nullptr, 0));
  if (argc > 2) num_items = static_cast<unsigned>(std::strtoul(argv[2], nullptr, 0));
  if (argc > 3) goal = std::strtod(argv[3], nullptr);
  uvm::uvm_config_db<unsigned>::set(nullptr, "*", "seed", seed);
  uvm::uvm_config_db<unsigned>::set(nullptr, "*", "num_items", num_items);
  uvm::uvm_config_db<double>::set(nullptr, "*", "goal", goal);

  uvm::run_test("bus_cov_test");

  uvm::uvm_report_server* svr = uvm::uvm_report_server::get_server();
  return (svr->get_severity_count(uvm::UVM_ERROR) + svr->get_severity_count(uvm::UVM_FATAL)) ? 1 : 0;
}
