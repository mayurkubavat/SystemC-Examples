// ----------------------------------------------------------------------------
// alu_sequences — fully random and inline-constrained stimulus
//
// alu_random_sequence    : randomize() with only the class constraints
//                          (SV: `uvm_do(req)`)
// alu_mul_corner_sequence: randomize_with() adds inline constraints on top
//                          (SV: `uvm_do_with(req, { op == MUL; a == 15; })`)
//
// The sequence follows the explicit create / start_item / randomize /
// finish_item pattern so every step is visible. crave2uvm.h also redefines
// UVM_DO / UVM_DO_WITH to call CRAVE's randomize for you.
// ----------------------------------------------------------------------------
#ifndef ALU_SEQUENCES_H
#define ALU_SEQUENCES_H

#include <systemc>
#include <uvm>

#include "alu_item.h"

class alu_random_sequence : public uvm::uvm_sequence<alu_item> {
public:
  unsigned num_items = 200;

  UVM_OBJECT_UTILS(alu_random_sequence);

  alu_random_sequence(const std::string& name = "alu_random_sequence")
    : uvm::uvm_sequence<alu_item>(name) {}

  void body() {
    for (unsigned i = 0; i < num_items; ++i) {
      alu_item* item = alu_item::type_id::create("item");
      start_item(item);
      if (!item->randomize())
        UVM_ERROR(get_name(), "randomize() failed");
      finish_item(item);
    }
  }
};

class alu_mul_corner_sequence : public uvm::uvm_sequence<alu_item> {
public:
  unsigned num_items = 20;

  UVM_OBJECT_UTILS(alu_mul_corner_sequence);

  alu_mul_corner_sequence(const std::string& name = "alu_mul_corner_sequence")
    : uvm::uvm_sequence<alu_item>(name) {}

  void body() {
    for (unsigned i = 0; i < num_items; ++i) {
      alu_item* item = alu_item::type_id::create("item");
      start_item(item);
      // Inline constraints are solved together with the class constraints,
      // so b still obeys c_mul_small (b < 16).
      if (!item->randomize_with(item->op() == alu_op_e::MUL, item->a() == 15))
        UVM_ERROR(get_name(), "randomize_with() failed");
      alu_op_e op = item->op;
      if (op != +alu_op_e::MUL || static_cast<sc_dt::sc_uint<8> >(item->a) != 15)
        UVM_ERROR(get_name(), "inline constraint ignored: " + item->convert2string());
      finish_item(item);
    }
  }
};

#endif // ALU_SEQUENCES_H
