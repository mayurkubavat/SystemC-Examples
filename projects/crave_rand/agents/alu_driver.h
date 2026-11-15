// ----------------------------------------------------------------------------
// alu_driver — consumes randomized ALU items and checks them
//
// There is no pin-level DUT here: the point of this example is the
// stimulus. The driver plays the role of a reference model, computing the
// ALU result, and checks every class constraint held. A solver bug or a
// missing constraint shows up as a UVM_ERROR (e.g. divide by zero).
//
// It also keeps a histogram of opcodes so you can see the solver spread
// values across the legal space, and prints it in report_phase.
// ----------------------------------------------------------------------------
#ifndef ALU_DRIVER_H
#define ALU_DRIVER_H

#include <systemc>
#include <uvm>

#include <map>
#include <sstream>

#include "alu_item.h"

class alu_driver : public uvm::uvm_driver<alu_item> {
public:
  UVM_COMPONENT_UTILS(alu_driver);

  unsigned num_items = 0;
  unsigned num_violations = 0;
  std::map<std::string, unsigned> op_hist;

  alu_driver(uvm::uvm_component_name name)
    : uvm::uvm_driver<alu_item>(name) {}

  void run_phase(uvm::uvm_phase& phase) {
    while (true) {
      alu_item item;
      seq_item_port->get_next_item(item);
      check_and_execute(item);
      sc_core::wait(10, sc_core::SC_NS);  // one "cycle" per operation
      seq_item_port->item_done();
    }
  }

  void report_phase(uvm::uvm_phase& phase) {
    std::ostringstream ss;
    ss << num_items << " items, " << num_violations << " constraint violations; ops:";
    for (const auto& kv : op_hist) ss << " " << kv.first << "=" << kv.second;
    UVM_INFO(get_name(), ss.str(), uvm::UVM_LOW);
  }

private:
  void check_and_execute(const alu_item& item) {
    alu_op_e op = item.op;
    unsigned a = static_cast<sc_dt::sc_uint<8> >(item.a);
    unsigned b = static_cast<sc_dt::sc_uint<8> >(item.b);
    unsigned result = 0;
    bool ok = true;

    switch (op) {
      case alu_op_e::ADD: result = a + b; break;
      case alu_op_e::SUB: ok = (a >= b); result = a - b; break;
      case alu_op_e::MUL: ok = (a < 16 && b < 16); result = a * b; break;
      case alu_op_e::DIV: ok = (b != 0); result = ok ? a / b : 0; break;
    }

    ++num_items;
    ++op_hist[op._to_string()];
    if (!ok) {
      ++num_violations;
      UVM_ERROR(get_name(), "constraint violated: " + item.convert2string());
    }

    std::ostringstream ss;
    ss << item.convert2string() << " -> " << result;
    UVM_INFO(get_name(), ss.str(), uvm::UVM_HIGH);
  }
};

#endif // ALU_DRIVER_H
