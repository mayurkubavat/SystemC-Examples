// ----------------------------------------------------------------------------
// bus_item — a simple read/write bus transaction
//
// This example is about measuring coverage, not generating stimulus, so the
// item uses plain fields and the sequence randomizes them with <random>.
// (See projects/crave_rand for solver-based constrained-random stimulus.)
// ----------------------------------------------------------------------------
#ifndef BUS_ITEM_H
#define BUS_ITEM_H

#include <systemc>
#include <uvm>

#include <sstream>

enum bus_cmd_t { BUS_READ = 0, BUS_WRITE = 1 };

class bus_item : public uvm::uvm_sequence_item {
public:
  int cmd = BUS_READ;  // bus_cmd_t
  int addr = 0;         // 0..255
  int len = 1;          // burst length in beats, 1..16

  UVM_OBJECT_UTILS(bus_item);

  bus_item(const std::string& name = "bus_item") : uvm::uvm_sequence_item(name) {}

  virtual void do_copy(const uvm::uvm_object& rhs) {
    const bus_item* rhs_ = dynamic_cast<const bus_item*>(&rhs);
    if (!rhs_) UVM_FATAL("do_copy", "cast failed");
    uvm::uvm_sequence_item::do_copy(rhs);
    cmd = rhs_->cmd;
    addr = rhs_->addr;
    len = rhs_->len;
  }

  std::string convert2string() const {
    std::ostringstream ss;
    ss << (cmd == BUS_WRITE ? "WRITE" : "READ ") << " addr=" << addr << " len=" << len;
    return ss.str();
  }
};

#endif // BUS_ITEM_H
