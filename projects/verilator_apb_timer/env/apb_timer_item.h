#ifndef APB_TIMER_ITEM_H
#define APB_TIMER_ITEM_H

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

#include <crave2uvm.h>
#include <crave/ConstrainedRandom.hpp>
#include <crave/experimental/Experimental.hpp>
#include <systemc>
#include <uvm>

class apb_timer_item : public uvm_randomized_sequence_item {
 public:
  crave::crv_variable<uint32_t> addr{"addr"};
  crave::crv_variable<uint32_t> data{"data"};
  crave::crv_variable<bool> write{"write"};

  // Sampled response fields populated by driver/monitor
  bool error = false;
  bool irq_after = false;

  // Named hard constraints on legal APB timer register accesses
  crave::crv_constraint c_word_aligned{"c_word_aligned", (addr() & 0x3u) == 0u};
  crave::crv_constraint c_reg_window{"c_reg_window", addr() <= 0x10u};
  crave::crv_constraint c_ctrl_clean{
      "c_ctrl_clean",
      crave::if_then(write() && (addr() == 0x00u), (data() & ~0x37u) == 0u)};
  crave::crv_constraint c_fast_load{
      "c_fast_load",
      crave::if_then(write() && (addr() == 0x04u),
                     data() >= 1u && data() <= 6u)};
  crave::crv_constraint c_byte_payload{
      "c_byte_payload",
      crave::if_then(write() && (addr() == 0x10u), data() <= 0xFFu)};

  // Named soft default: prefer writes unless overridden inline
  crave::crv_soft_constraint c_prefer_write{"c_prefer_write", write() == true};

  UVM_OBJECT_UTILS(apb_timer_item);

  apb_timer_item(crave::crv_object_name name = "apb_timer_item")
      : uvm_randomized_sequence_item(name) {
    addr = 0u;
    data = 0u;
    write = false;
  }

  virtual void do_copy(const uvm::uvm_object& rhs) {
    const auto* r = dynamic_cast<const apb_timer_item*>(&rhs);
    if (!r) UVM_FATAL("COPY", "apb_timer_item::do_copy cast failed");
    uvm::uvm_sequence_item::do_copy(rhs);
    addr = static_cast<uint32_t>(r->addr);
    data = static_cast<uint32_t>(r->data);
    write = static_cast<bool>(r->write);
    error = r->error;
    irq_after = r->irq_after;
  }

  virtual bool do_compare(const uvm::uvm_object& rhs,
                          const uvm::uvm_comparer*) const {
    const auto* r = dynamic_cast<const apb_timer_item*>(&rhs);
    if (!r) return false;
    return static_cast<uint32_t>(addr) == static_cast<uint32_t>(r->addr) &&
           static_cast<uint32_t>(data) == static_cast<uint32_t>(r->data) &&
           static_cast<bool>(write) == static_cast<bool>(r->write) &&
           error == r->error;
  }

  virtual std::string convert2string() const {
    std::ostringstream os;
    os << (static_cast<bool>(write) ? "WR" : "RD")
       << " addr=0x" << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<uint32_t>(addr)
       << " data=0x" << std::setw(8) << std::setfill('0')
       << static_cast<uint32_t>(data) << std::dec
       << " err=" << error << " irq=" << irq_after;
    return os.str();
  }
};

#endif // APB_TIMER_ITEM_H
