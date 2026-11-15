// ----------------------------------------------------------------------------
// top.cpp — sc_main for the CRAVE constrained-random example
//
// crave::init() starts the constraint solver backend and reads an optional
// crave.cfg from the working directory (seed, solver choice). The seed can
// also be set in code with crave::set_global_seed(); pass it on the command
// line to reproduce a run:  ./crave_rand_sim 1234
// ----------------------------------------------------------------------------
#include <systemc>
#include <uvm>

#include <cstdlib>

#include "alu_rand_test.h"

int sc_main(int argc, char* argv[]) {
  crave::init();
  if (argc > 1)
    crave::set_global_seed(static_cast<unsigned>(std::strtoul(argv[1], nullptr, 0)));

  uvm::run_test("alu_rand_test");

  uvm::uvm_report_server* svr = uvm::uvm_report_server::get_server();
  return (svr->get_severity_count(uvm::UVM_ERROR) + svr->get_severity_count(uvm::UVM_FATAL)) ? 1 : 0;
}
