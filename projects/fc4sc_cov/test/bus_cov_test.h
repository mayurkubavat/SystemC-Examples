// ----------------------------------------------------------------------------
// bus_cov_test — random bus traffic with FC4SC coverage closure check
//
// Testbench:
//   bus_cov_test
//    +-- bus_sequencer
//    +-- bus_driver    --ap-->  bus_coverage (uvm_subscriber + FC4SC covergroup)
//
// Pass criteria: zero UVM_ERRORs, which includes reaching the coverage goal.
// ----------------------------------------------------------------------------
#ifndef BUS_COV_TEST_H
#define BUS_COV_TEST_H

#include <systemc>
#include <uvm>

#include "bus_driver.h"
#include "bus_coverage.h"
#include "bus_random_sequence.h"

class bus_sequencer : public uvm::uvm_sequencer<bus_item> {
public:
  UVM_COMPONENT_UTILS(bus_sequencer);
  bus_sequencer(uvm::uvm_component_name name) : uvm::uvm_sequencer<bus_item>(name) {}
};

class bus_cov_test : public uvm::uvm_test {
public:
  UVM_COMPONENT_UTILS(bus_cov_test);

  bus_sequencer* sequencer = nullptr;
  bus_driver* driver = nullptr;
  bus_coverage* coverage = nullptr;

  bus_cov_test(uvm::uvm_component_name name) : uvm::uvm_test(name) {}

  void build_phase(uvm::uvm_phase& phase) {
    uvm::uvm_test::build_phase(phase);
    sequencer = bus_sequencer::type_id::create("sequencer", this);
    driver = bus_driver::type_id::create("driver", this);
    coverage = bus_coverage::type_id::create("coverage", this);
  }

  void connect_phase(uvm::uvm_phase& phase) {
    driver->seq_item_port.connect(sequencer->seq_item_export);
    driver->ap.connect(coverage->analysis_export);
  }

  void run_phase(uvm::uvm_phase& phase) {
    phase.raise_objection(this);
    bus_random_sequence* seq = bus_random_sequence::type_id::create("seq");
    seq->start(sequencer);
    phase.drop_objection(this);
  }

  void final_phase(uvm::uvm_phase& phase) {
    uvm::uvm_report_server* svr = uvm::uvm_report_server::get_server();
    if (svr->get_severity_count(uvm::UVM_ERROR) + svr->get_severity_count(uvm::UVM_FATAL) == 0) {
      UVM_INFO(get_name(), "** TEST PASSED **", uvm::UVM_NONE);
    } else {
      UVM_INFO(get_name(), "** TEST FAILED **", uvm::UVM_NONE);
    }
  }
};

#endif // BUS_COV_TEST_H
