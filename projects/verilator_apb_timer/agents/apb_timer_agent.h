#ifndef APB_TIMER_AGENT_H
#define APB_TIMER_AGENT_H

#include <systemc>
#include <uvm>

#include "apb_timer_if.h"
#include "apb_timer_item.h"

class apb_timer_sequencer : public uvm::uvm_sequencer<apb_timer_item> {
 public:
  UVM_COMPONENT_UTILS(apb_timer_sequencer);

  explicit apb_timer_sequencer(uvm::uvm_component_name name)
      : uvm::uvm_sequencer<apb_timer_item>(name) {}
};

class apb_timer_driver : public uvm::uvm_driver<apb_timer_item> {
 public:
  UVM_COMPONENT_UTILS(apb_timer_driver);

  apb_timer_if* vif = nullptr;
  sc_core::sc_clock* clk = nullptr;
  sc_core::sc_signal<bool>* resetn = nullptr;

  explicit apb_timer_driver(uvm::uvm_component_name name)
      : uvm::uvm_driver<apb_timer_item>(name) {}

  virtual void build_phase(uvm::uvm_phase& phase) {
    uvm::uvm_driver<apb_timer_item>::build_phase(phase);
    if (!uvm::uvm_config_db<apb_timer_if*>::get(this, "", "vif", vif))
      UVM_FATAL("NOVIF", "Missing apb_timer_if in config_db");
    if (!uvm::uvm_config_db<sc_core::sc_clock*>::get(this, "", "clk", clk))
      UVM_FATAL("NOCLK", "Missing sc_clock in config_db");
    if (!uvm::uvm_config_db<sc_core::sc_signal<bool>*>::get(this, "", "resetn", resetn))
      UVM_FATAL("NORST", "Missing resetn in config_db");
  }

  virtual void run_phase(uvm::uvm_phase&) {
    vif->PSEL.write(false);
    vif->PENABLE.write(false);
    vif->PWRITE.write(false);
    vif->PADDR.write(0u);
    vif->PWDATA.write(0u);

    while (!resetn->read())
      sc_core::wait(clk->posedge_event());

    while (true) {
      apb_timer_item req;
      seq_item_port->get_next_item(req);

      // Drive SETUP on negedge so signals are rock-solid before posedge #1
      sc_core::wait(clk->negedge_event());
      vif->PSEL.write(true);
      vif->PENABLE.write(false);
      vif->PADDR.write(static_cast<uint32_t>(req.addr));
      vif->PWRITE.write(static_cast<bool>(req.write));
      vif->PWDATA.write(static_cast<bool>(req.write)
                            ? static_cast<uint32_t>(req.data)
                            : 0u);

      // Drive ACCESS on next negedge so PSEL && PENABLE hold across posedge #2
      sc_core::wait(clk->negedge_event());
      vif->PENABLE.write(true);

      // Sample settled response on negedge after posedge #2, then return IDLE
      sc_core::wait(clk->negedge_event());
      if (!static_cast<bool>(req.write))
        req.data = vif->PRDATA.read();
      req.error = vif->PSLVERR.read();
      req.irq_after = vif->IRQ.read();

      vif->PSEL.write(false);
      vif->PENABLE.write(false);

      apb_timer_item rsp;
      rsp.copy(req);
      rsp.set_id_info(req);
      seq_item_port->item_done(rsp);
    }
  }
};

class apb_timer_monitor : public uvm::uvm_monitor {
 public:
  UVM_COMPONENT_UTILS(apb_timer_monitor);

  uvm::uvm_analysis_port<apb_timer_item> ap{"ap"};
  apb_timer_if* vif = nullptr;
  sc_core::sc_clock* clk = nullptr;
  sc_core::sc_signal<bool>* resetn = nullptr;
  unsigned observed_txns = 0;

  explicit apb_timer_monitor(uvm::uvm_component_name name)
      : uvm::uvm_monitor(name) {}

  virtual void build_phase(uvm::uvm_phase& phase) {
    uvm::uvm_monitor::build_phase(phase);
    if (!uvm::uvm_config_db<apb_timer_if*>::get(this, "", "vif", vif))
      UVM_FATAL("NOVIF", "Missing apb_timer_if in config_db");
    if (!uvm::uvm_config_db<sc_core::sc_clock*>::get(this, "", "clk", clk))
      UVM_FATAL("NOCLK", "Missing sc_clock in config_db");
    if (!uvm::uvm_config_db<sc_core::sc_signal<bool>*>::get(this, "", "resetn", resetn))
      UVM_FATAL("NORST", "Missing resetn in config_db");
  }

  virtual void run_phase(uvm::uvm_phase&) {
    while (true) {
      sc_core::wait(clk->posedge_event());
      if (!resetn->read()) continue;
      const bool sel = vif->PSEL.read();
      const bool ena = vif->PENABLE.read();
      if (!(sel && ena)) continue;

      const uint32_t addr = vif->PADDR.read();
      const bool wr = vif->PWRITE.read();
      const uint32_t wdata = vif->PWDATA.read();

      // Wait 1ns past posedge for Verilator nonblocking updates to settle
      sc_core::wait(1, sc_core::SC_NS);
      apb_timer_item obs("obs");
      obs.addr = addr;
      obs.write = wr;
      obs.data = wr ? wdata : vif->PRDATA.read();
      obs.error = vif->PSLVERR.read();
      obs.irq_after = vif->IRQ.read();
      ++observed_txns;
      ap.write(obs);
    }
  }
};

class apb_timer_agent : public uvm::uvm_agent {
 public:
  UVM_COMPONENT_UTILS(apb_timer_agent);

  apb_timer_sequencer* sqr = nullptr;
  apb_timer_driver* drv = nullptr;
  apb_timer_monitor* mon = nullptr;

  explicit apb_timer_agent(uvm::uvm_component_name name)
      : uvm::uvm_agent(name) {}

  virtual void build_phase(uvm::uvm_phase& phase) {
    uvm::uvm_agent::build_phase(phase);
    mon = apb_timer_monitor::type_id::create("mon", this);
    if (get_is_active() == uvm::UVM_ACTIVE) {
      sqr = apb_timer_sequencer::type_id::create("sqr", this);
      drv = apb_timer_driver::type_id::create("drv", this);
    }
  }

  virtual void connect_phase(uvm::uvm_phase& phase) {
    uvm::uvm_agent::connect_phase(phase);
    if (get_is_active() == uvm::UVM_ACTIVE)
      drv->seq_item_port.connect(sqr->seq_item_export);
  }
};

#endif // APB_TIMER_AGENT_H
