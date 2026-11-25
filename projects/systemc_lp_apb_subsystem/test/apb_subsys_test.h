#ifndef APB_SUBSYS_TEST_H
#define APB_SUBSYS_TEST_H

#include <sstream>

#include <crave2uvm.h>
#include <fc4sc.hpp>
#include <fc4sc_sva.hpp>
#include <systemc>
#include <uvm>
#include <systemc-lp>

#include "apb_pmu_timer_subsys.h"
#include "apb_subsys_env.h"

#ifndef LP_BUG
#define LP_BUG 0
#endif

class apb_subsys_test : public uvm::uvm_test {
public:
    UVM_COMPONENT_UTILS(apb_subsys_test);

    apb_subsys_env*        env = nullptr;
    apb_pmu_timer_subsys*  dut = nullptr;
    sc_core::sc_clock*     clk = nullptr;
    fc4sc::sva_handle      h_sva_iso;

    explicit apb_subsys_test(uvm::uvm_component_name name)
      : uvm::uvm_test(name) {}

    void build_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_test::build_phase(phase);
        env = apb_subsys_env::type_id::create("env", this);
        uvm::uvm_config_db<apb_pmu_timer_subsys*>::get(this, "", "dut", dut);
        uvm::uvm_config_db<sc_core::sc_clock*>::get(this, "", "clk", clk);
    }

    void start_of_simulation_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_test::start_of_simulation_phase(phase);
        if (dut && clk) {
            // Preponed temporal SVA: whenever timer power is disabled, isolation MUST be asserted
            fc4sc::sva_options opts;
            opts.no_throw = true;
            h_sva_iso = FC4SC_ASSERT_PROPERTY(
                "SVA_PMU_ISO_WHEN_PWR_OFF",
                fc4sc::sva::posedge(*clk),
                fc4sc::sva::implies(
                    fc4sc::sva::pred([this]() { return !dut->tim_pwr_en.read(); }),
                    fc4sc::sva::pred([this]() { return dut->tim_iso_en.read(); })),
                opts);
        }
    }

    // PMU register bit helper: [0]=pwr_en, [1]=iso_en, [2]=save, [3]=restore, [4]=por_n
    void write_pmu(uint32_t val) {
        uvm::uvm_status_e st;
        env->reg_model->pmu_ctrl->write(st, val);
    }

    // Synchronize supply rail ramp with the APB commit posedge of a PMU write
    void write_pmu_with_dvfs(uint32_t val, double vdd_aon_v) {
        sc_core::sc_spawn([this, vdd_aon_v]() {
            do {
                sc_core::wait(clk->posedge_event());
            } while (!(env->drv->vif->PSEL.read() &&
                       env->drv->vif->PENABLE.read() &&
                       env->drv->vif->PWRITE.read()));
            sc_lp::upf::supply_on("VDD_AON", vdd_aon_v);
        });
        write_pmu(val);
    }

    void run_retention_power_cycle() {
        uvm::uvm_status_e st;
        uvm::uvm_reg_data_t rd = 0;

#if LP_BUG == 1
        // BUG 1: Cut power BEFORE asserting isolation!
        write_pmu(0x14u); // pwr_en=0, iso_en=0, save=1, por_n=1
        sc_core::wait(20, sc_core::SC_NS);
        write_pmu(0x12u); // iso_en=1 too late!
#elif LP_BUG == 2
        // BUG 2: Isolate & cut power FIRST, then pulse save while domain is OFF!
        write_pmu(0x12u); // pwr_en=0, iso_en=1 (no save yet)
        sc_core::wait(20, sc_core::SC_NS);
        write_pmu(0x16u); // save=1 while PD_TIM is already CORRUPT!
#else
        // Clean IEEE 1801 power-down sequence:
        // 1. Quiesce / Save retained CSRs while FULL_ON
        write_pmu(0x15u); // pwr_en=1, iso_en=0, save=1, por_n=1
        // 2. Assert isolation
        write_pmu(0x13u); // pwr_en=1, iso_en=1, por_n=1
        // 3. Cut primary switch power & assert domain POR reset low
        write_pmu(0x02u); // pwr_en=0, iso_en=1, por_n=0
#endif

        sc_core::wait(30, sc_core::SC_NS);

        // Verify APB firewall returns PSLVERR while isolated
        env->reg_model->tim_load->read(st, rd);
        if (dut->tim_iso_en.read() && st != uvm::UVM_NOT_OK) {
            UVM_ERROR("FW_CHK", "Expected APB firewall PSLVERR while PD_TIM is isolated");
        }

        // Clean Wakeup sequence:
        // 1. Restore primary power while holding POR low & isolation high
        write_pmu(0x03u); // pwr_en=1, iso_en=1, por_n=0
        sc_core::wait(20, sc_core::SC_NS);
        // 2. Pulse retention restore
        write_pmu(0x1Bu); // pwr_en=1, iso_en=1, restore=1, por_n=1
        // 3. Deassert isolation
        write_pmu(0x11u); // pwr_en=1, iso_en=0, por_n=1
        sc_core::wait(10, sc_core::SC_NS);
    }

    void run_phase(uvm::uvm_phase& phase) override {
        phase.raise_objection(this);
        sc_core::wait(25, sc_core::SC_NS);

        uvm::uvm_status_e   st;
        uvm::uvm_reg_data_t rd = 0;
        const bool          lp_on = sc_lp::upf::is_enabled();

        // 1. Program retained (LOAD=0x0024, CFG=0x03) and non-retained (SCRATCH=0xCAFE) CSRs
        env->reg_model->tim_load->write(st, 0x0024u);
        env->reg_model->tim_cfg->write(st, 0x03u);
        env->reg_model->tim_scratch->write(st, 0xCAFEu);
        sc_core::wait(20, sc_core::SC_NS);

        // 2. Constrained-random PST & DVFS sweeps (hits ALL 3 PST states & ALL 6 transitions!)
#if defined(CRAVE_CONSTRAINED_RANDOM_HPP) || defined(CRAVE2UVM_H)
        sc_lp::bridge::sclp_pst_rand_item pst_item("pst_item");
        pst_item.num_pst_states = 3;
        for (int i = 0; i < 6; ++i) {
            (void)pst_item.randomize();
        }
#endif

        // Exercise ALL_ON_NOM -> DVFS_LOW_VOLT -> ALL_ON_NOM
        sc_lp::upf::supply_on("VDD_AON", 0.75);
        sc_core::wait(20, sc_core::SC_NS);
        sc_lp::upf::supply_on("VDD_AON", 0.90);
        sc_core::wait(20, sc_core::SC_NS);

        // Exercise ALL_ON_NOM -> TIM_POWER_GATED -> ALL_ON_NOM (with Retention!)
        run_retention_power_cycle();

        // Exercise DVFS_LOW_VOLT <-> TIM_POWER_GATED transitions for 100% PST transition matrix!
        sc_lp::upf::supply_on("VDD_AON", 0.75);
        sc_core::wait(15, sc_core::SC_NS);
        // Sleep atomically from DVFS_LOW_VOLT -> TIM_POWER_GATED
        write_pmu(0x15u);
        write_pmu(0x13u);
        write_pmu_with_dvfs(0x02u, 0.90);
        sc_core::wait(20, sc_core::SC_NS);
        // Wake atomically from TIM_POWER_GATED -> DVFS_LOW_VOLT
        write_pmu_with_dvfs(0x03u, 0.75);
        sc_core::wait(15, sc_core::SC_NS);
        write_pmu(0x1Bu);
        write_pmu(0x11u);
        sc_core::wait(15, sc_core::SC_NS);
        sc_lp::upf::supply_on("VDD_AON", 0.90);
        sc_core::wait(15, sc_core::SC_NS);

        // 3. Post-wakeup verification of Retained vs. Non-Retained CSRs!
        env->reg_model->tim_load->read(st, rd);
        if (rd != 0x0024u) {
            UVM_ERROR("RET_LOAD", "Retained TIM_LOAD CSR did not preserve 0x0024 across power collapse");
        }
        env->reg_model->tim_cfg->read(st, rd);
        if (rd != 0x03u) {
            UVM_ERROR("RET_CFG", "Retained TIM_CFG CSR did not preserve 0x03 across power collapse");
        }

#if LP_BUG == 3
        // BUG 3: Firmware reads volatile non-retained TIM_SCRATCH after collapse without re-init!
        env->reg_model->tim_scratch->read(st, rd);
        if (rd != 0xCAFEu) {
            UVM_ERROR("VOL_SCRATCH", "Read corrupted non-retained TIM_SCRATCH without re-initialization");
        }
#else
        // Clean software flow: re-initialize non-retained scratch CSR after wakeup before reading!
        if (lp_on) {
            env->reg_model->tim_scratch->write(st, 0xBEEFu);
            env->reg_model->tim_scratch->read(st, rd);
            if (rd != 0xBEEFu) {
                UVM_ERROR("REINIT_ERR", "Re-initialized TIM_SCRATCH mismatch");
            }
        } else {
            // When LP is swapped out / disabled, TIM_SCRATCH was never corrupted!
            env->reg_model->tim_scratch->read(st, rd);
            if (rd != 0xCAFEu) {
                UVM_ERROR("FUNC_ERR", "Expected TIM_SCRATCH == 0xCAFE when LP is disabled");
            }
        }
#endif

        // 4. Verify 100% UPF PST State & Transition Coverage when LP is enabled!
        if (lp_on) {
            const double st_cov = sc_lp::bridge::sclp_fc4sc_manager::instance().get_pst_state_coverage("SUBSYS_PST");
            const double tr_cov = sc_lp::bridge::sclp_fc4sc_manager::instance().get_pst_transition_coverage("SUBSYS_PST");
            if (st_cov < 100.0 || tr_cov < 100.0) {
                std::ostringstream oss;
                oss << "Did not achieve 100% UPF PST coverage (state=" << st_cov
                    << "%, trans=" << tr_cov << "%)";
                UVM_ERROR("PST_COV", oss.str());
            }
        }

        fc4sc::sc_flush_sva();
        phase.drop_objection(this);
    }
};

#endif // APB_SUBSYS_TEST_H
