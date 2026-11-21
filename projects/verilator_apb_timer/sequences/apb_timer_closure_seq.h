#ifndef APB_TIMER_CLOSURE_SEQ_H
#define APB_TIMER_CLOSURE_SEQ_H

#include <cstdint>
#include <random>
#include <string>

#include <crave2uvm.h>
#include <fc4sc.hpp>
#include <systemc>
#include <uvm>

#include "apb_timer_item.h"
#include "apb_timer_ral.h"

// Coverage-driven stimulus sequence supporting two modes for paper comparison:
//   mode == "crave":         CRAVE SMT constrained-random + RAL frontdoor sequences
//   mode == "unconstrained": Uniform 32-bit random APB address/data/direction
class apb_timer_closure_seq : public uvm::uvm_sequence<apb_timer_item> {
 public:
  UVM_OBJECT_UTILS(apb_timer_closure_seq);

  apb_timer_reg_block* reg_model = nullptr;
  std::string mode = "crave";
  unsigned seed = 1u;
  unsigned max_txns = 200u;
  double target_cov = 100.0;
  unsigned txns_executed = 0u;

  explicit apb_timer_closure_seq(const std::string& name = "apb_timer_closure_seq")
      : uvm::uvm_sequence<apb_timer_item>(name) {}

  void send_bus(uint32_t addr, uint32_t data, bool wr, apb_timer_item& rsp) {
    auto* req = apb_timer_item::type_id::create("req");
    start_item(req);
    req->addr = addr;
    req->data = data;
    req->write = wr;
    finish_item(req);
    get_response(&rsp);
    ++txns_executed;
  }

  virtual void body() {
    std::mt19937 rng(seed);
    apb_timer_item rsp("rsp");

    if (mode == "unconstrained") {
      std::uniform_int_distribution<uint32_t> dist_addr(0u, 0x3Fu);
      std::uniform_int_distribution<uint32_t> dist_data(0u, 0xFFFFFFFFu);
      std::uniform_int_distribution<int> dist_wr(0, 1);

      while (txns_executed < max_txns &&
             fc4sc::global::get_coverage() < target_cov) {
        // Mask word alignment half the time to model naive random bus traffic
        const uint32_t raw_addr = dist_addr(rng);
        const uint32_t d = dist_data(rng);
        const bool wr = (dist_wr(rng) != 0);
        send_bus(raw_addr, d, wr, rsp);
      }
      return;
    }

    if (mode == "crave-only") {
      apb_timer_item crv_item("crv_item");
      while (txns_executed < max_txns &&
             fc4sc::global::get_coverage() < target_cov) {
        const bool force_read = ((txns_executed % 2u) == 1u);
        bool ok = force_read ? crv_item.randomize_with(crv_item.write() == false)
                             : crv_item.randomize();
        if (!ok) {
          UVM_ERROR("CRV_RAND", "Unexpected UNSAT in apb_timer_item::randomize()");
          break;
        }
        send_bus(static_cast<uint32_t>(crv_item.addr),
                 static_cast<uint32_t>(crv_item.data),
                 static_cast<bool>(crv_item.write),
                 rsp);
        // Allow enabled timer to tick a few cycles periodically
        if ((txns_executed % 8u) == 0u)
          sc_core::wait(40, sc_core::SC_NS);
      }
      return;
    }

    // Mode "crave" (CRAVE constrained-random + UVM RAL closure):
    // Phase 1: 24 CRAVE constrained-random bus transactions
    {
      apb_timer_item crv_item("crv_item");
      for (unsigned i = 0; i < 24u && txns_executed < max_txns; ++i) {
        const bool force_read = ((i % 2u) == 1u);
        bool ok = force_read ? crv_item.randomize_with(crv_item.write() == false)
                             : crv_item.randomize();
        if (!ok) {
          UVM_ERROR("CRV_RAND", "Unexpected UNSAT in apb_timer_item::randomize()");
          break;
        }
        send_bus(static_cast<uint32_t>(crv_item.addr),
                 static_cast<uint32_t>(crv_item.data),
                 static_cast<bool>(crv_item.write),
                 rsp);
      }
    }

    // Phase 2: UVM RAL frontdoor walking patterns on SCRATCH & LOAD,
    // exercising uvm_fc4sc_reg bit-toggle and field-value covergroups.
    uvm::uvm_status_e status;
    uvm::uvm_reg_data_t rdata;

    const uint32_t scratch_patterns[] = {0x00u, 0xFFu, 0xAAu, 0x55u, 0x01u, 0x80u, 0x00u};
    for (uint32_t pat : scratch_patterns) {
      if (txns_executed >= max_txns) break;
      reg_model->scratch->write(status, pat, uvm::UVM_FRONTDOOR, nullptr, this);
      ++txns_executed;
      if (txns_executed >= max_txns) break;
      reg_model->scratch->read(status, rdata, uvm::UVM_FRONTDOOR, nullptr, this);
      ++txns_executed;
    }

    const uint32_t load_patterns[] = {0x00u, 0x80u, 0xFFu, 0x02u};
    for (uint32_t pat : load_patterns) {
      if (txns_executed >= max_txns) break;
      reg_model->load->write(status, pat, uvm::UVM_FRONTDOOR, nullptr, this);
      ++txns_executed;
      if (txns_executed >= max_txns) break;
      reg_model->load->read(status, rdata, uvm::UVM_FRONTDOOR, nullptr, this);
      ++txns_executed;
      if (txns_executed >= max_txns) break;
      reg_model->count->read(status, rdata, uvm::UVM_FRONTDOOR, nullptr, this);
      ++txns_executed;
    }

    // Exercise invalid address PSLVERR bin once intentionally
    if (txns_executed < max_txns) {
      send_bus(0x18u, 0x0u, false, rsp);
    }

    // Phase 2: Exercise all (prescale x periodic) timer modes with short load=2
    // so terminal count, IRQ assertion, COUNT==0/mid/max, and W1C clearing close
    // deterministically while testing prescaler & periodic reload logic.
    reg_model->ctrl->write(status, 0x00u, uvm::UVM_FRONTDOOR, nullptr, this);
    ++txns_executed;
    reg_model->load->write(status, 0xFFu, uvm::UVM_FRONTDOOR, nullptr, this);
    ++txns_executed;
    reg_model->count->read(status, rdata, uvm::UVM_FRONTDOOR, nullptr, this);
    ++txns_executed;

    for (uint32_t pre = 0; pre < 4 && txns_executed < max_txns; ++pre) {
      for (uint32_t per = 0; per < 2 && txns_executed < max_txns; ++per) {
        // Stop timer, program LOAD=2, clear pending IRQ
        reg_model->ctrl->write(status, 0x00u, uvm::UVM_FRONTDOOR, nullptr, this);
        ++txns_executed;
        reg_model->load->write(status, 0x02u, uvm::UVM_FRONTDOOR, nullptr, this);
        ++txns_executed;
        reg_model->irq_status->write(status, 0x01u, uvm::UVM_FRONTDOOR, nullptr, this);
        ++txns_executed;

        const uint32_t ctrl_val = (pre << 4) | (1u << 2) | (per << 1) | 1u;
        reg_model->ctrl->write(status, ctrl_val, uvm::UVM_FRONTDOOR, nullptr, this);
        ++txns_executed;

        // Wait enough clock cycles for terminal count (2 * 2^pre + 2 cycles)
        const unsigned wait_ns = (2u * (1u << pre) + 3u) * 10u;
        sc_core::wait(static_cast<double>(wait_ns), sc_core::SC_NS);

        // Read COUNT and IRQ_STATUS, then test W1C with 0 (no-op) and 1 (clear)
        reg_model->count->read(status, rdata, uvm::UVM_FRONTDOOR, nullptr, this);
        ++txns_executed;
        reg_model->irq_status->read(status, rdata, uvm::UVM_FRONTDOOR, nullptr, this);
        ++txns_executed;
        reg_model->irq_status->write(status, 0x00u, uvm::UVM_FRONTDOOR, nullptr, this);
        ++txns_executed;
        reg_model->irq_status->read(status, rdata, uvm::UVM_FRONTDOOR, nullptr, this);
        ++txns_executed;
        reg_model->irq_status->write(status, 0x01u, uvm::UVM_FRONTDOOR, nullptr, this);
        ++txns_executed;
        reg_model->irq_status->read(status, rdata, uvm::UVM_FRONTDOOR, nullptr, this);
        ++txns_executed;
      }
    }

    // Ensure CTRL bits toggle 1->0 and are read in both states
    if (txns_executed < max_txns) {
      reg_model->ctrl->write(status, 0x37u, uvm::UVM_FRONTDOOR, nullptr, this);
      ++txns_executed;
      reg_model->ctrl->read(status, rdata, uvm::UVM_FRONTDOOR, nullptr, this);
      ++txns_executed;
      reg_model->ctrl->write(status, 0x00u, uvm::UVM_FRONTDOOR, nullptr, this);
      ++txns_executed;
      reg_model->ctrl->read(status, rdata, uvm::UVM_FRONTDOOR, nullptr, this);
      ++txns_executed;
    }

    // Phase 3: CRAVE constrained-random exploration until target_cov or max_txns
    apb_timer_item crv_item("crv_item");
    while (txns_executed < max_txns &&
           fc4sc::global::get_coverage() < target_cov) {
      const bool force_read = ((txns_executed % 2u) == 1u);
      bool ok = false;
      if (force_read) {
        ok = crv_item.randomize_with(crv_item.write() == false);
      } else {
        ok = crv_item.randomize();
      }
      if (!ok) {
        UVM_ERROR("CRV_RAND", "Unexpected UNSAT in apb_timer_item::randomize()");
        break;
      }
      send_bus(static_cast<uint32_t>(crv_item.addr),
               static_cast<uint32_t>(crv_item.data),
               static_cast<bool>(crv_item.write),
               rsp);
    }
  }
};

#endif // APB_TIMER_CLOSURE_SEQ_H
