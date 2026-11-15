// ----------------------------------------------------------------------------
// alu_rand_test — runs fully random and inline-constrained ALU stimulus
//
// Testbench:
//   alu_rand_test
//    +-- alu_sequencer
//    +-- alu_driver  (reference model + constraint checker)
//
// Pass criteria: zero UVM_ERRORs (no constraint violations, no failed
// randomize() calls, inline constraints honored).
// ----------------------------------------------------------------------------
#ifndef ALU_RAND_TEST_H
#define ALU_RAND_TEST_H

#include <systemc>
#include <uvm>

#include "alu_driver.h"
#include "alu_sequences.h"

// UVM-SystemC needs a named subclass to register the sequencer with the
// factory (SV: typedef uvm_sequencer#(alu_item) alu_sequencer;)
class alu_sequencer : public uvm::uvm_sequencer<alu_item> {
public:
  UVM_COMPONENT_UTILS(alu_sequencer);
  alu_sequencer(uvm::uvm_component_name name) : uvm::uvm_sequencer<alu_item>(name) {}
};

class alu_rand_test : public uvm::uvm_test {
public:
  UVM_COMPONENT_UTILS(alu_rand_test);

  alu_sequencer* sequencer = nullptr;
  alu_driver* driver = nullptr;

  alu_rand_test(uvm::uvm_component_name name) : uvm::uvm_test(name) {}

  void build_phase(uvm::uvm_phase& phase) {
    uvm::uvm_test::build_phase(phase);
    sequencer = alu_sequencer::type_id::create("sequencer", this);
    driver = alu_driver::type_id::create("driver", this);
  }

  void connect_phase(uvm::uvm_phase& phase) {
    driver->seq_item_port.connect(sequencer->seq_item_export);
  }

  void run_phase(uvm::uvm_phase& phase) {
    phase.raise_objection(this);

    alu_random_sequence* rnd = alu_random_sequence::type_id::create("rnd");
    rnd->start(sequencer);

    alu_mul_corner_sequence* mul = alu_mul_corner_sequence::type_id::create("mul");
    mul->start(sequencer);

    phase.drop_objection(this);
  }

  void report_phase(uvm::uvm_phase& phase) {
    uvm::uvm_report_server* svr = uvm::uvm_report_server::get_server();
    if (svr->get_severity_count(uvm::UVM_ERROR) + svr->get_severity_count(uvm::UVM_FATAL) == 0) {
      UVM_INFO(get_name(), "** TEST PASSED **", uvm::UVM_NONE);
    } else {
      UVM_INFO(get_name(), "** TEST FAILED **", uvm::UVM_NONE);
    }
  }
};

#endif // ALU_RAND_TEST_H
