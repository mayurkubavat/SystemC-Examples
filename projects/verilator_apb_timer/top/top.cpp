#include <cstdlib>
#include <cstring>
#include <string>

#include <crave2uvm.h>
#include <fc4sc.hpp>
#include <systemc>
#include <uvm>

#include "Vapb_timer.h"
#include "apb_timer_if.h"
#include "apb_timer_test.h"

SC_MODULE(apb_timer_reset_gen) {
  sc_core::sc_in<bool> clk{"clk"};
  sc_core::sc_out<bool> resetn{"resetn"};

  SC_CTOR(apb_timer_reset_gen) {
    SC_THREAD(run);
    sensitive << clk.pos();
  }

  void run() {
    resetn.write(false);
    wait();
    wait();
    resetn.write(true);
  }
};

int sc_main(int argc, char* argv[]) {
  std::string mode = "crave";
  unsigned seed = 1u;
  unsigned max_txns = 200u;
  double target_cov = 100.0;
  std::string ucis_out = "verilator_apb_timer_ucis.xml";

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
      mode = argv[++i];
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 0));
    } else if (std::strcmp(argv[i], "--max-txns") == 0 && i + 1 < argc) {
      max_txns = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 0));
    } else if (std::strcmp(argv[i], "--goal") == 0 && i + 1 < argc) {
      target_cov = std::strtod(argv[++i], nullptr);
    } else if (std::strcmp(argv[i], "--ucis") == 0 && i + 1 < argc) {
      ucis_out = argv[++i];
    }
  }

  crave::init();
  crave::set_global_seed(seed);

  sc_core::sc_clock clk("clk", 10, sc_core::SC_NS);
  sc_core::sc_signal<bool> resetn("resetn");
  apb_timer_if vif("vif");

  Vapb_timer dut("dut");
  dut.PCLK(clk);
  dut.PRESETn(resetn);
  dut.PSEL(vif.PSEL);
  dut.PENABLE(vif.PENABLE);
  dut.PWRITE(vif.PWRITE);
  dut.PADDR(vif.PADDR);
  dut.PWDATA(vif.PWDATA);
  dut.PRDATA(vif.PRDATA);
  dut.PREADY(vif.PREADY);
  dut.PSLVERR(vif.PSLVERR);
  dut.IRQ(vif.IRQ);

  apb_timer_reset_gen rst("rst");
  rst.clk(clk);
  rst.resetn(resetn);

  uvm::uvm_config_db<apb_timer_if*>::set(nullptr, "*", "vif", &vif);
  uvm::uvm_config_db<Vapb_timer*>::set(nullptr, "*", "dut", &dut);
  uvm::uvm_config_db<sc_core::sc_clock*>::set(nullptr, "*", "clk", &clk);
  uvm::uvm_config_db<sc_core::sc_signal<bool>*>::set(nullptr, "*", "resetn", &resetn);
  uvm::uvm_config_db<std::string>::set(nullptr, "*", "mode", mode);
  uvm::uvm_config_db<unsigned>::set(nullptr, "*", "seed", seed);
  uvm::uvm_config_db<unsigned>::set(nullptr, "*", "max_txns", max_txns);
  uvm::uvm_config_db<double>::set(nullptr, "*", "target_cov", target_cov);
  uvm::uvm_config_db<std::string>::set(nullptr, "*", "ucis_out", ucis_out);

  uvm::run_test("apb_timer_test");

  uvm::uvm_report_server* svr = uvm::uvm_report_server::get_server();
  return (svr->get_severity_count(uvm::UVM_ERROR) +
          svr->get_severity_count(uvm::UVM_FATAL))
             ? 1
             : 0;
}
