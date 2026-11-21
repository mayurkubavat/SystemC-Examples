#ifndef APB_TIMER_COVERAGE_H
#define APB_TIMER_COVERAGE_H

#include <cstdint>

#include <fc4sc.hpp>
#include <systemc>
#include <uvm>

#include "apb_timer_item.h"

using fc4sc::bin;
using fc4sc::covergroup;
using fc4sc::coverpoint;
using fc4sc::cross;
using fc4sc::interval;

class apb_timer_arch_cg : public fc4sc::covergroup {
 public:
  uint32_t addr = 0;
  uint32_t dir = 0;
  uint32_t prescale = 0;
  uint32_t periodic = 0;
  uint32_t irq = 0;
  uint32_t slverr = 0;

  CG_CONS(apb_timer_arch_cg) {
    x_prescale_mode.option().cross_num_print_missing = 100;
  }

  void sample_bus(uint32_t a, bool wr, bool err, bool irq_pin, uint8_t last_ctrl) {
    addr = a;
    dir = wr ? 1u : 0u;
    prescale = (last_ctrl >> 4) & 0x3u;
    periodic = (last_ctrl >> 1) & 0x1u;
    irq = irq_pin ? 1u : 0u;
    slverr = err ? 1u : 0u;
    covergroup::sample();
  }

  COVERPOINT(uint32_t, cp_addr, addr) {
    fc4sc::bin<uint32_t>("ctrl", 0x00u),
    fc4sc::bin<uint32_t>("load", 0x04u),
    fc4sc::bin<uint32_t>("count", 0x08u),
    fc4sc::bin<uint32_t>("irq_status", 0x0Cu),
    fc4sc::bin<uint32_t>("scratch", 0x10u),
    fc4sc::bin<uint32_t>("invalid", fc4sc::interval(0x14u, 0xFFu))
  };

  COVERPOINT(uint32_t, cp_dir, dir) {
    fc4sc::bin<uint32_t>("read", 0u),
    fc4sc::bin<uint32_t>("write", 1u)
  };

  COVERPOINT(uint32_t, cp_prescale, prescale) {
    fc4sc::bin<uint32_t>("div1", 0u),
    fc4sc::bin<uint32_t>("div2", 1u),
    fc4sc::bin<uint32_t>("div4", 2u),
    fc4sc::bin<uint32_t>("div8", 3u)
  };

  COVERPOINT(uint32_t, cp_mode, periodic) {
    fc4sc::bin<uint32_t>("oneshot", 0u),
    fc4sc::bin<uint32_t>("periodic", 1u)
  };

  COVERPOINT(uint32_t, cp_irq, irq) {
    fc4sc::bin<uint32_t>("deasserted", 0u),
    fc4sc::bin<uint32_t>("asserted", 1u)
  };

  COVERPOINT(uint32_t, cp_slverr, slverr) {
    fc4sc::bin<uint32_t>("ok", 0u),
    fc4sc::bin<uint32_t>("error", 1u)
  };

  fc4sc::cross<uint32_t, uint32_t> x_prescale_mode =
      fc4sc::cross<uint32_t, uint32_t>(this, "x_prescale_mode", &cp_prescale, &cp_mode);
};

class apb_timer_coverage : public uvm::uvm_subscriber<apb_timer_item> {
 public:
  UVM_COMPONENT_UTILS(apb_timer_coverage);

  apb_timer_arch_cg cg{"apb_timer_arch_cg"};
  uint8_t tracked_ctrl = 0;

  explicit apb_timer_coverage(uvm::uvm_component_name name)
      : uvm::uvm_subscriber<apb_timer_item>(name) {}

  virtual void write(const apb_timer_item& t) {
    if (!t.error && static_cast<bool>(t.write) && static_cast<uint32_t>(t.addr) == 0x00u)
      tracked_ctrl = static_cast<uint8_t>(static_cast<uint32_t>(t.data) & 0x37u);
    cg.sample_bus(static_cast<uint32_t>(t.addr),
                  static_cast<bool>(t.write),
                  t.error,
                  t.irq_after,
                  tracked_ctrl);
  }
};

#endif // APB_TIMER_COVERAGE_H
