#ifndef APB_TIMER_TEST_H
#define APB_TIMER_TEST_H

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include <crave2uvm.h>
#include <fc4sc.hpp>
#include <systemc>
#include <uvm>

#include "Vapb_timer.h"
#include "Vapb_timer___024root.h"

#include "apb_timer_closure_seq.h"
#include "apb_timer_env.h"

#ifndef BUG_SEED
#define BUG_SEED 0
#endif

class apb_timer_test : public uvm::uvm_test {
 public:
  UVM_COMPONENT_UTILS(apb_timer_test);

  apb_timer_env* env = nullptr;
  Vapb_timer* dut = nullptr;
  sc_core::sc_signal<bool>* resetn = nullptr;
  sc_core::sc_clock* clk = nullptr;

  std::string mode = "crave";
  unsigned seed = 1u;
  unsigned max_txns = 200u;
  double target_cov = 100.0;
  std::string ucis_out = "verilator_apb_timer_ucis.xml";

  unsigned executed_txns = 0u;
  double wall_ms = 0.0;

  explicit apb_timer_test(uvm::uvm_component_name name) : uvm::uvm_test(name) {}

  virtual void build_phase(uvm::uvm_phase& phase) {
    uvm::uvm_test::build_phase(phase);
    env = apb_timer_env::type_id::create("env", this);

    if (!uvm::uvm_config_db<Vapb_timer*>::get(this, "", "dut", dut))
      UVM_FATAL("NODUT", "Missing Vapb_timer* in config_db");
    if (!uvm::uvm_config_db<sc_core::sc_signal<bool>*>::get(this, "", "resetn", resetn))
      UVM_FATAL("NORST", "Missing resetn in config_db");
    if (!uvm::uvm_config_db<sc_core::sc_clock*>::get(this, "", "clk", clk))
      UVM_FATAL("NOCLK", "Missing clk in config_db");

    uvm::uvm_config_db<std::string>::get(this, "", "mode", mode);
    uvm::uvm_config_db<unsigned>::get(this, "", "seed", seed);
    uvm::uvm_config_db<unsigned>::get(this, "", "max_txns", max_txns);
    uvm::uvm_config_db<double>::get(this, "", "target_cov", target_cov);
    uvm::uvm_config_db<std::string>::get(this, "", "ucis_out", ucis_out);
  }

  virtual void end_of_elaboration_phase(uvm::uvm_phase& phase) {
    uvm::uvm_test::end_of_elaboration_phase(phase);
    // Bind UVM RAL backdoors directly to Verilator's public_flat_rw C++ storage!
    uvm::uvm_bind_cpp_backdoor(env->reg_model->ctrl,
                               dut->rootp->apb_timer__DOT__ctrl_q);
    uvm::uvm_bind_cpp_backdoor(env->reg_model->load,
                               dut->rootp->apb_timer__DOT__load_q);
    uvm::uvm_bind_cpp_backdoor(env->reg_model->count,
                               dut->rootp->apb_timer__DOT__count_q,
                               false);
    uvm::uvm_bind_cpp_backdoor(env->reg_model->irq_status,
                               dut->rootp->apb_timer__DOT__irq_status_q);
    uvm::uvm_bind_cpp_backdoor(env->reg_model->scratch,
                               dut->rootp->apb_timer__DOT__scratch_q);
  }

  void verify_crave_diagnostics() {
    apb_timer_item diag_item("diag_item");
    // 1. Deliberately violate c_word_aligned + c_reg_window via randomize_with
    bool sat = diag_item.randomize_with(diag_item.addr() == 0x23u);
    if (sat) {
      UVM_ERROR("CRV_DIAG", "Expected UNSAT for unaligned out-of-range inline constraint");
    } else {
      auto muses = diag_item.analyse_contradiction();
      if (muses.empty())
        UVM_ERROR("CRV_DIAG", "analyse_contradiction() returned empty MUS set on UNSAT");
    }

    // 2. Override soft constraint c_prefer_write (write() == true) with write() == false
    bool sat2 = diag_item.randomize_with(diag_item.write() == false);
    if (!sat2) {
      UVM_ERROR("CRV_DIAG", "Expected SAT when overriding soft constraint c_prefer_write");
    } else {
      auto inactive = diag_item.inactive_soft_constraints();
      if (inactive.empty())
        UVM_ERROR("CRV_DIAG", "inactive_soft_constraints() missed overridden c_prefer_write");
    }
  }

  void verify_verilator_backdoor() {
    uvm::uvm_status_e status = uvm::UVM_NOT_OK;
    uvm::uvm_reg_data_t val = 0;

    // Backdoor poke SCRATCH, verify RTL C++ storage + scoreboard sync + mirror check
    env->reg_model->scratch->poke(status, 0x5Au);
    if (status != uvm::UVM_IS_OK || dut->rootp->apb_timer__DOT__scratch_q != 0x5Au)
      UVM_ERROR("BKDR", "Verilator uvm_cpp_reg_backdoor poke on SCRATCH failed");
    env->scb->sync_from_backdoor(dut->rootp->apb_timer__DOT__ctrl_q,
                                 dut->rootp->apb_timer__DOT__load_q,
                                 dut->rootp->apb_timer__DOT__count_q,
                                 dut->rootp->apb_timer__DOT__irq_status_q,
                                 dut->rootp->apb_timer__DOT__scratch_q);
    env->reg_model->scratch->mirror(status, uvm::UVM_CHECK, uvm::UVM_BACKDOOR);
    if (status != uvm::UVM_IS_OK)
      UVM_ERROR("BKDR", "Verilator uvm_cpp_reg_backdoor mirror(UVM_CHECK) failed");

    // Reset SCRATCH back to 0 via backdoor poke
    env->reg_model->scratch->poke(status, 0x00u);
    env->scb->sync_from_backdoor(dut->rootp->apb_timer__DOT__ctrl_q,
                                 dut->rootp->apb_timer__DOT__load_q,
                                 dut->rootp->apb_timer__DOT__count_q,
                                 dut->rootp->apb_timer__DOT__irq_status_q,
                                 dut->rootp->apb_timer__DOT__scratch_q);

    // Peek RO COUNT register via backdoor
    env->reg_model->count->peek(status, val);
    if (status != uvm::UVM_IS_OK || val.to_uint64() != 0x0Fu)
      UVM_ERROR("BKDR", "Verilator backdoor peek on RO COUNT expected reset 0x0F");

    // Read Verilator SystemC sc_out ports directly via uvm_hdl_read + bit slice
    bool irq_pin = true;
    uint32_t prdata_low = 0xFFu;
    if (!uvm::uvm_hdl_read("dut.IRQ", irq_pin) || irq_pin)
      UVM_ERROR("HDL_RD", "uvm_hdl_read(\"dut.IRQ\") failed after reset");
    if (!uvm::uvm_hdl_read("dut.PRDATA[7:0]", prdata_low) || prdata_low != 0u)
      UVM_ERROR("HDL_RD", "uvm_hdl_read(\"dut.PRDATA[7:0]\") failed after reset");
  }

  virtual void run_phase(uvm::uvm_phase& phase) {
    phase.raise_objection(this);

    const auto t0 = std::chrono::steady_clock::now();

    while (!resetn->read())
      sc_core::wait(clk->posedge_event());
    sc_core::wait(clk->negedge_event());

    verify_crave_diagnostics();
    verify_verilator_backdoor();

    auto* seq = apb_timer_closure_seq::type_id::create("seq");
    seq->reg_model = env->reg_model;
    seq->mode = mode;
    seq->seed = seed;
    seq->max_txns = max_txns;
    seq->target_cov = target_cov;
    seq->start(env->agt->sqr);
    executed_txns = seq->txns_executed;

    // Final end-of-test backdoor mirror check on stable non-volatile CSRs
    if (BUG_SEED == 0) {
      uvm::uvm_status_e status = uvm::UVM_NOT_OK;
      env->reg_model->load->mirror(status, uvm::UVM_CHECK, uvm::UVM_BACKDOOR);
      env->reg_model->scratch->mirror(status, uvm::UVM_CHECK, uvm::UVM_BACKDOOR);
    }

    const auto t1 = std::chrono::steady_clock::now();
    wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    phase.drop_objection(this);
  }

  virtual void report_phase(uvm::uvm_phase& phase) {
    uvm::uvm_test::report_phase(phase);
    xml_printer::coverage_save(ucis_out);

    const double total_cov = fc4sc::global::get_coverage();
    const double arch_cov = env->cov->cg.get_inst_coverage();
    const double addr_cov = env->reg_model->get_addr_map_coverage();
    auto* svr = uvm::uvm_report_server::get_server();
    const int errs = svr->get_severity_count(uvm::UVM_ERROR) +
                     svr->get_severity_count(uvm::UVM_FATAL);

    std::cout << std::fixed << std::setprecision(2)
              << "BENCHMARK_SUMMARY"
              << " mode=" << mode
              << " seed=" << seed
              << " bug_seed=" << BUG_SEED
              << " txns=" << executed_txns
              << " cycles=" << env->scb->cycle_count
              << " first_fail_cycle=" << env->scb->first_fail_cycle
              << " addr_cov=" << addr_cov
              << " arch_cov=" << arch_cov
              << " total_cov=" << total_cov
              << " wall_ms=" << wall_ms
              << " errors=" << errs
              << std::endl;
  }
};

#endif // APB_TIMER_TEST_H
