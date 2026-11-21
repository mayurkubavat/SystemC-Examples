#ifndef APB_TIMER_SCOREBOARD_H
#define APB_TIMER_SCOREBOARD_H

#include <cstdint>
#include <sstream>

#include <systemc>
#include <uvm>

#include "apb_timer_if.h"

class apb_timer_scoreboard : public uvm::uvm_scoreboard {
 public:
  UVM_COMPONENT_UTILS(apb_timer_scoreboard);

  apb_timer_if* vif = nullptr;
  sc_core::sc_clock* clk = nullptr;
  sc_core::sc_signal<bool>* resetn = nullptr;

  // Cycle-accurate golden architectural state
  uint8_t ref_ctrl = 0x00u;
  uint8_t ref_load = 0x0Fu;
  uint8_t ref_count = 0x0Fu;
  uint8_t ref_irq_status = 0x00u;
  uint8_t ref_scratch = 0x00u;
  uint8_t ref_pre_cnt = 0x00u;

  uint64_t cycle_count = 0;
  uint64_t checked_reads = 0;
  uint64_t checked_irqs = 0;
  uint64_t first_fail_cycle = 0;

  explicit apb_timer_scoreboard(uvm::uvm_component_name name)
      : uvm::uvm_scoreboard(name) {}

  virtual void build_phase(uvm::uvm_phase& phase) {
    uvm::uvm_scoreboard::build_phase(phase);
    if (!uvm::uvm_config_db<apb_timer_if*>::get(this, "", "vif", vif))
      UVM_FATAL("NOVIF", "Missing apb_timer_if in config_db");
    if (!uvm::uvm_config_db<sc_core::sc_clock*>::get(this, "", "clk", clk))
      UVM_FATAL("NOCLK", "Missing sc_clock in config_db");
    if (!uvm::uvm_config_db<sc_core::sc_signal<bool>*>::get(this, "", "resetn", resetn))
      UVM_FATAL("NORST", "Missing resetn in config_db");
  }

  void sync_from_backdoor(uint8_t ctrl, uint8_t load, uint8_t count,
                          uint8_t irq_st, uint8_t scratch) {
    ref_ctrl = ctrl & 0x37u;
    ref_load = load;
    ref_count = count;
    ref_irq_status = irq_st & 0x1u;
    ref_scratch = scratch;
    ref_pre_cnt = 0u;
  }

  static uint8_t prescale_limit(uint8_t ctrl) {
    switch ((ctrl >> 4) & 0x3u) {
      case 0: return 0u;
      case 1: return 1u;
      case 2: return 3u;
      default: return 7u;
    }
  }

  virtual void run_phase(uvm::uvm_phase&) {
    while (true) {
      sc_core::wait(clk->posedge_event());

      if (!resetn->read()) {
        ref_ctrl = 0x00u;
        ref_load = 0x0Fu;
        ref_count = 0x0Fu;
        ref_irq_status = 0x00u;
        ref_scratch = 0x00u;
        ref_pre_cnt = 0x00u;
        continue;
      }

      ++cycle_count;

      // Sample inputs present at this posedge
      const bool apb_acc = vif->PSEL.read() && vif->PENABLE.read();
      const bool pwrite = vif->PWRITE.read();
      const uint32_t paddr = vif->PADDR.read();
      const uint32_t pwdata = vif->PWDATA.read();

      const uint8_t pre_ctrl = ref_ctrl;
      const uint8_t pre_load = ref_load;
      const uint8_t pre_count = ref_count;
      const uint8_t pre_irq = ref_irq_status;
      const uint8_t pre_scratch = ref_scratch;

      const bool cfg_en = (pre_ctrl & 0x1u) != 0u;
      const bool cfg_periodic = (pre_ctrl & 0x2u) != 0u;
      const uint8_t pre_lim = prescale_limit(pre_ctrl);
      const bool tick = cfg_en && (ref_pre_cnt == pre_lim);

      // 1. Default timer progression
      if (cfg_en) {
        if (ref_pre_cnt == pre_lim) {
          ref_pre_cnt = 0u;
          if (pre_count == 1u) {
            ref_irq_status = 1u;
            if (cfg_periodic) {
              ref_count = pre_load;
            } else {
              ref_count = 0u;
              ref_ctrl &= static_cast<uint8_t>(~0x1u);
            }
          } else if (pre_count == 0u) {
            if (cfg_periodic) {
              ref_count = pre_load;
            } else {
              ref_ctrl &= static_cast<uint8_t>(~0x1u);
            }
          } else {
            ref_count = static_cast<uint8_t>(pre_count - 1u);
          }
        } else {
          ref_pre_cnt = static_cast<uint8_t>(ref_pre_cnt + 1u);
        }
      } else {
        ref_pre_cnt = 0u;
      }

      // 2. APB access overrides
      bool exp_err = false;
      uint32_t exp_prdata = 0u;
      const bool addr_ok = ((paddr & 0x3u) == 0u) && (paddr <= 0x10u);

      if (apb_acc) {
        if (!addr_ok) {
          exp_err = true;
          exp_prdata = 0u;
        } else if (pwrite) {
          switch (paddr & 0xFFu) {
            case 0x00u:
              ref_ctrl = static_cast<uint8_t>(pwdata & 0x37u);
              ref_pre_cnt = 0u;
              break;
            case 0x04u:
              ref_load = static_cast<uint8_t>(pwdata & 0xFFu);
              if (!cfg_en)
                ref_count = static_cast<uint8_t>(pwdata & 0xFFu);
              break;
            case 0x08u:
              break;
            case 0x0Cu:
              if ((pwdata & 0x1u) != 0u)
                ref_irq_status = (tick && (pre_count == 1u)) ? 1u : 0u;
              break;
            case 0x10u:
              ref_scratch = static_cast<uint8_t>(pwdata & 0xFFu);
              break;
            default:
              exp_err = true;
              break;
          }
        } else {
          switch (paddr & 0xFFu) {
            case 0x00u: exp_prdata = pre_ctrl; break;
            case 0x04u: exp_prdata = pre_load; break;
            case 0x08u: exp_prdata = pre_count; break;
            case 0x0Cu: exp_prdata = pre_irq; break;
            case 0x10u: exp_prdata = pre_scratch; break;
            default:    exp_prdata = 0u; break;
          }
        }
      }

      // Wait 1ns past posedge for Verilator NBA updates to settle
      sc_core::wait(1, sc_core::SC_NS);

      const bool exp_irq_pin = (((ref_ctrl >> 2) & 0x1u) != 0u) && (ref_irq_status != 0u);
      const bool act_irq_pin = vif->IRQ.read();
      ++checked_irqs;
      if (act_irq_pin != exp_irq_pin) {
        if (first_fail_cycle == 0) first_fail_cycle = cycle_count;
        std::ostringstream msg;
        msg << "Cycle " << cycle_count << ": IRQ mismatch: RTL=" << act_irq_pin
            << " expected=" << exp_irq_pin
            << " (ctrl=0x" << std::hex << static_cast<unsigned>(ref_ctrl)
            << " count=0x" << static_cast<unsigned>(ref_count)
            << " irq_st=" << static_cast<unsigned>(ref_irq_status) << std::dec << ")";
        UVM_ERROR("SCB_IRQ", msg.str());
      }

      if (apb_acc) {
        const bool act_err = vif->PSLVERR.read();
        if (act_err != exp_err) {
          if (first_fail_cycle == 0) first_fail_cycle = cycle_count;
          std::ostringstream msg;
          msg << "Cycle " << cycle_count << ": PSLVERR mismatch at addr=0x"
              << std::hex << paddr << std::dec << ": RTL=" << act_err
              << " expected=" << exp_err;
          UVM_ERROR("SCB_ERR", msg.str());
        }
        if (!pwrite && !exp_err) {
          ++checked_reads;
          const uint32_t act_prdata = vif->PRDATA.read();
          if (act_prdata != exp_prdata) {
            if (first_fail_cycle == 0) first_fail_cycle = cycle_count;
            std::ostringstream msg;
            msg << "Cycle " << cycle_count << ": PRDATA mismatch at addr=0x"
                << std::hex << paddr << ": RTL=0x" << act_prdata
                << " expected=0x" << exp_prdata << std::dec;
            UVM_ERROR("SCB_DATA", msg.str());
          }
        }
      }
    }
  }
};

#endif // APB_TIMER_SCOREBOARD_H
