// ----------------------------------------------------------------------------
// alu_item — constrained-random UVM sequence item built on CRAVE
//
// UVM-SystemC has no built-in randomize(). CRAVE (Constrained RAndom
// Verification Environment) fills that gap, and its crave2uvm adapter
// provides uvm_randomized_sequence_item, which inherits from both
// uvm::uvm_sequence_item and crave::crv_sequence_item.
//
//   SV:  class alu_item extends uvm_sequence_item;
//          rand alu_op_e op;  rand bit [7:0] a, b;
//          constraint c_div { op == DIV -> b != 0; }
//   SC:  class alu_item : public uvm_randomized_sequence_item {
//          crave::crv_variable<alu_op_e> op;  crave::crv_variable<sc_uint<8>> a, b;
//          crave::crv_constraint c_div{ crave::if_then(op() == DIV, b() != 0) };
//
// crv_variable is the "rand" field. crv_constraint members are class
// constraints, solved together on every randomize() call. Inside a
// constraint, write var() (with parentheses) to get the solver expression.
// ----------------------------------------------------------------------------
#ifndef ALU_ITEM_H
#define ALU_ITEM_H

#include <systemc>
#include <uvm>
#include <crave2uvm.h>
#include <crave/ConstrainedRandom.hpp>
#include <crave/experimental/Experimental.hpp>

#include <sstream>

// CRAVE_BETTER_ENUM declares an enum the solver can reason about
// (SV: typedef enum {ADD, SUB, MUL, DIV} alu_op_e;)
CRAVE_BETTER_ENUM(alu_op_e, ADD = 0, SUB = 1, MUL = 2, DIV = 3);

class alu_item : public uvm_randomized_sequence_item {
public:
  crave::crv_variable<alu_op_e> op;
  crave::crv_variable<sc_dt::sc_uint<8> > a;
  crave::crv_variable<sc_dt::sc_uint<8> > b;

  // Never divide by zero (SV: op == DIV -> b != 0;)
  crave::crv_constraint c_div_nonzero{ crave::if_then(op() == alu_op_e::DIV, b() != 0) };

  // Keep subtraction results unsigned (SV: op == SUB -> a >= b;)
  crave::crv_constraint c_sub_no_underflow{ crave::if_then(op() == alu_op_e::SUB, a() >= b()) };

  // Keep MUL operands small so the product fits in 8 bits
  // (SV: op == MUL -> a < 16 && b < 16;)
  crave::crv_constraint c_mul_small{ crave::if_then(op() == alu_op_e::MUL, a() < 16 && b() < 16) };

  UVM_OBJECT_UTILS(alu_item);

  alu_item(crave::crv_object_name name = "alu_item")
    : uvm_randomized_sequence_item(name) {
    op = alu_op_e::ADD;
    a = 0;
    b = 0;
  }

  // CRAVE has no field macros, so copy explicitly. The driver receives
  // items by value, which calls do_copy.
  virtual void do_copy(const uvm::uvm_object& rhs) {
    const alu_item* rhs_ = dynamic_cast<const alu_item*>(&rhs);
    if (!rhs_) UVM_FATAL("do_copy", "cast failed");
    uvm::uvm_sequence_item::do_copy(rhs);
    op = static_cast<alu_op_e>(rhs_->op);
    a = static_cast<sc_dt::sc_uint<8> >(rhs_->a);
    b = static_cast<sc_dt::sc_uint<8> >(rhs_->b);
  }

  std::string convert2string() const {
    std::ostringstream ss;
    alu_op_e o = op;
    ss << o._to_string() << " a=" << static_cast<sc_dt::sc_uint<8> >(a)
       << " b=" << static_cast<sc_dt::sc_uint<8> >(b);
    return ss.str();
  }
};

#endif // ALU_ITEM_H
