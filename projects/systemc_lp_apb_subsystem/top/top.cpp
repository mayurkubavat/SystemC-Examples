// Single unmodified testbench entry point supporting:
//   1. Functional mode (LP OFF, 0% overhead): ./systemc_lp_apb_subsys_sim
//   2. Full IEEE 1801 UPF mode (same binary): ./systemc_lp_apb_subsys_sim --upf apb_subsys.upf
//   3. Link-time stub mode (0 Tcl):           linked against SystemC::lp_stub

#include <crave2uvm.h>
#include <fc4sc.hpp>
#include <fc4sc_sva.hpp>
#include <systemc>
#include <uvm>
#include <systemc-lp>

#include "apb_pmu_timer_subsys.h"
#include "apb_subsys_if.h"
#include "apb_subsys_test.h"

SC_MODULE(subsys_reset_gen) {
    sc_core::sc_in<bool>  clk{"clk"};
    sc_core::sc_out<bool> resetn{"resetn"};

    SC_CTOR(subsys_reset_gen) {
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

int sc_main(int, char*[]) {
    crave::init();
    crave::set_global_seed(42u);
    sc_lp::upf::set_corrupt_policy(sc_lp::corrupt_policy_e::SEEDED_SCRAMBLE, 42ULL);

    sc_core::sc_clock        clk("clk", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool> resetn("resetn");
    apb_subsys_if            vif("vif");

    apb_pmu_timer_subsys dut("dut");
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

    subsys_reset_gen rst("rst");
    rst.clk(clk);
    rst.resetn(resetn);

    uvm::uvm_config_db<apb_subsys_if*>::set(nullptr, "*", "vif", &vif);
    uvm::uvm_config_db<apb_pmu_timer_subsys*>::set(nullptr, "*", "dut", &dut);
    uvm::uvm_config_db<sc_core::sc_clock*>::set(nullptr, "*", "clk", &clk);
    uvm::uvm_config_db<sc_core::sc_signal<bool>*>::set(nullptr, "*", "resetn", &resetn);

    // Promote SCLP & SVA errors into clean exit failure counts without uncaught throws
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR,             sc_core::SC_DISPLAY);
    sc_core::sc_report_handler::set_actions(sc_lp::SCLP_ID_ISO_VIOLATION_, sc_core::SC_DISPLAY);
    sc_core::sc_report_handler::set_actions(sc_lp::SCLP_ID_RET_HAZARD_,    sc_core::SC_DISPLAY);
    sc_core::sc_report_handler::set_actions(sc_lp::SCLP_ID_PST_ILLEGAL_,   sc_core::SC_DISPLAY);
    sc_core::sc_report_handler::set_actions(sc_lp::SCLP_ID_RAL_CORRUPT_,   sc_core::SC_DISPLAY);

    uvm::run_test("apb_subsys_test");

    uvm::uvm_report_server* svr = uvm::uvm_report_server::get_server();
    const unsigned sc_lp_errs =
        sc_core::sc_report_handler::get_count(sc_lp::SCLP_ID_ISO_VIOLATION_) +
        sc_core::sc_report_handler::get_count(sc_lp::SCLP_ID_RET_HAZARD_) +
        sc_core::sc_report_handler::get_count(sc_lp::SCLP_ID_PST_ILLEGAL_) +
        sc_core::sc_report_handler::get_count(sc_lp::SCLP_ID_RAL_CORRUPT_) +
        sc_core::sc_report_handler::get_count(sc_core::SC_ERROR);

    return (svr->get_severity_count(uvm::UVM_ERROR) +
            svr->get_severity_count(uvm::UVM_FATAL) +
            sc_lp_errs)
               ? 1
               : 0;
}
