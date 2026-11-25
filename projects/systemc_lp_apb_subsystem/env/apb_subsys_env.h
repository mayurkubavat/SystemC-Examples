#ifndef APB_SUBSYS_ENV_H
#define APB_SUBSYS_ENV_H

#include <cstdint>
#include <string>

#include <crave2uvm.h>
#include <crave/ConstrainedRandom.hpp>
#include <fc4sc.hpp>
#include <systemc>
#include <uvm>

#include "apb_subsys_if.h"

class apb_subsys_item : public uvm_randomized_sequence_item {
public:
    crave::crv_variable<uint32_t> addr{"addr"};
    crave::crv_variable<uint32_t> data{"data"};
    crave::crv_variable<bool>     write{"write"};
    bool                          slverr = false;

    crave::crv_constraint c_aligned{
        "c_aligned",
        (addr() & 0x3u) == 0u,
        addr() <= 0x1Cu
    };

    UVM_OBJECT_UTILS(apb_subsys_item);

    apb_subsys_item(crave::crv_object_name name = "apb_subsys_item")
      : uvm_randomized_sequence_item(name) {
        addr  = 0u;
        data  = 0u;
        write = false;
    }

    void do_copy(const uvm::uvm_object& rhs) override {
        const auto* r = dynamic_cast<const apb_subsys_item*>(&rhs);
        if (!r) return;
        uvm::uvm_sequence_item::do_copy(rhs);
        addr   = static_cast<uint32_t>(r->addr);
        data   = static_cast<uint32_t>(r->data);
        write  = static_cast<bool>(r->write);
        slverr = r->slverr;
    }
};

class apb_subsys_sequencer : public uvm::uvm_sequencer<apb_subsys_item> {
public:
    UVM_COMPONENT_UTILS(apb_subsys_sequencer);
    explicit apb_subsys_sequencer(uvm::uvm_component_name name)
      : uvm::uvm_sequencer<apb_subsys_item>(name) {}
};

class apb_subsys_driver : public uvm::uvm_driver<apb_subsys_item> {
public:
    UVM_COMPONENT_UTILS(apb_subsys_driver);
    apb_subsys_if*             vif    = nullptr;
    sc_core::sc_clock*         clk    = nullptr;
    sc_core::sc_signal<bool>*  resetn = nullptr;

    explicit apb_subsys_driver(uvm::uvm_component_name name)
      : uvm::uvm_driver<apb_subsys_item>(name) {}

    void build_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_driver<apb_subsys_item>::build_phase(phase);
        uvm::uvm_config_db<apb_subsys_if*>::get(this, "", "vif", vif);
        uvm::uvm_config_db<sc_core::sc_clock*>::get(this, "", "clk", clk);
        uvm::uvm_config_db<sc_core::sc_signal<bool>*>::get(this, "", "resetn", resetn);
    }

    void run_phase(uvm::uvm_phase&) override {
        vif->PSEL.write(false);
        vif->PENABLE.write(false);
        vif->PWRITE.write(false);
        vif->PADDR.write(0u);
        vif->PWDATA.write(0u);

        while (!resetn->read()) sc_core::wait(clk->posedge_event());

        while (true) {
            apb_subsys_item req;
            seq_item_port->get_next_item(req);

            sc_core::wait(clk->negedge_event());
            vif->PSEL.write(true);
            vif->PENABLE.write(false);
            vif->PADDR.write(static_cast<uint32_t>(req.addr));
            vif->PWRITE.write(static_cast<bool>(req.write));
            vif->PWDATA.write(static_cast<bool>(req.write) ? static_cast<uint32_t>(req.data) : 0u);

            sc_core::wait(clk->negedge_event());
            vif->PENABLE.write(true);

            sc_core::wait(clk->negedge_event());
            if (!static_cast<bool>(req.write)) req.data = vif->PRDATA.read();
            req.slverr = vif->PSLVERR.read();
            vif->PSEL.write(false);
            vif->PENABLE.write(false);

            apb_subsys_item rsp;
            rsp.copy(req);
            rsp.set_id_info(req);
            seq_item_port->item_done(rsp);
        }
    }
};

class apb_subsys_monitor : public uvm::uvm_monitor {
public:
    UVM_COMPONENT_UTILS(apb_subsys_monitor);
    uvm::uvm_analysis_port<apb_subsys_item> ap{"ap"};
    apb_subsys_if*                          vif = nullptr;
    sc_core::sc_clock*                      clk = nullptr;

    explicit apb_subsys_monitor(uvm::uvm_component_name name)
      : uvm::uvm_monitor(name) {}

    void build_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_monitor::build_phase(phase);
        uvm::uvm_config_db<apb_subsys_if*>::get(this, "", "vif", vif);
        uvm::uvm_config_db<sc_core::sc_clock*>::get(this, "", "clk", clk);
    }

    void run_phase(uvm::uvm_phase&) override {
        while (true) {
            sc_core::wait(clk->posedge_event());
            if (vif->PSEL.read() && vif->PENABLE.read() && vif->PREADY.read()) {
                apb_subsys_item obs;
                obs.addr   = vif->PADDR.read();
                obs.write  = vif->PWRITE.read();
                obs.data   = vif->PWRITE.read() ? vif->PWDATA.read() : vif->PRDATA.read();
                obs.slverr = vif->PSLVERR.read();
                ap.write(obs);
            }
        }
    }
};

// UVM RAL Registers (zero UPF code — sclp_uvm.h discovers HDL paths automatically!)
class simple_uvm_reg : public uvm::uvm_reg {
public:
    uvm::uvm_reg_field* value = nullptr;
    unsigned            width = 16;

    UVM_OBJECT_UTILS(simple_uvm_reg);

    explicit simple_uvm_reg(const std::string& name = "simple_uvm_reg", unsigned nbits = 16)
      : uvm::uvm_reg(name, nbits, uvm::UVM_NO_COVERAGE), width(nbits) {}

    void build(bool volatile_f = false, const std::string& acc = "RW") {
        value = uvm::uvm_reg_field::type_id::create("value");
        value->configure(this, width, 0, acc, volatile_f, 0x0, true, true, true);
    }
};

class apb_subsys_reg_block : public uvm::uvm_reg_block {
public:
    simple_uvm_reg* pmu_ctrl    = nullptr;
    simple_uvm_reg* pmu_stat    = nullptr;
    simple_uvm_reg* tim_cfg     = nullptr;
    simple_uvm_reg* tim_load    = nullptr;
    simple_uvm_reg* tim_scratch = nullptr;
    simple_uvm_reg* tim_count   = nullptr;

    UVM_OBJECT_UTILS(apb_subsys_reg_block);

    explicit apb_subsys_reg_block(const std::string& name = "apb_subsys_reg_block")
      : uvm::uvm_reg_block(name, uvm::UVM_NO_COVERAGE) {}

    void build() {
        configure(nullptr, "dut");
        default_map = create_map("default_map", 0x0, 4, uvm::UVM_LITTLE_ENDIAN);

        pmu_ctrl = new simple_uvm_reg("pmu_ctrl", 8);
        pmu_ctrl->configure(this, nullptr, "u_pmu.tim_pwr_en_q");
        pmu_ctrl->build(false, "RW");
        default_map->add_reg(pmu_ctrl, 0x00, "RW");

        pmu_stat = new simple_uvm_reg("pmu_stat", 8);
        pmu_stat->configure(this, nullptr, "tim_pwr_ack");
        pmu_stat->build(true, "RO");
        default_map->add_reg(pmu_stat, 0x04, "RO");

        tim_cfg = new simple_uvm_reg("tim_cfg", 8);
        tim_cfg->configure(this, nullptr, "u_timer.cfg_q");
        tim_cfg->build(false, "RW");
        default_map->add_reg(tim_cfg, 0x10, "RW");

        tim_load = new simple_uvm_reg("tim_load", 16);
        tim_load->configure(this, nullptr, "u_timer.load_q");
        tim_load->build(false, "RW");
        default_map->add_reg(tim_load, 0x14, "RW");

        tim_scratch = new simple_uvm_reg("tim_scratch", 16);
        tim_scratch->configure(this, nullptr, "u_timer.scratch_q");
        tim_scratch->build(false, "RW");
        default_map->add_reg(tim_scratch, 0x18, "RW");

        tim_count = new simple_uvm_reg("tim_count", 16);
        tim_count->configure(this, nullptr, "u_timer.count_q");
        tim_count->build(true, "RO");
        default_map->add_reg(tim_count, 0x1C, "RO");

        lock_model();
    }
};

class apb_subsys_reg_adapter : public uvm::uvm_reg_adapter {
public:
    UVM_OBJECT_UTILS(apb_subsys_reg_adapter);

    explicit apb_subsys_reg_adapter(const std::string& name = "apb_subsys_reg_adapter")
      : uvm::uvm_reg_adapter(name) {
        provides_responses = true;
    }

    uvm::uvm_sequence_item* reg2bus(const uvm::uvm_reg_bus_op& rw) override {
        auto* bus = apb_subsys_item::type_id::create("bus");
        bus->addr  = static_cast<uint32_t>(rw.addr);
        bus->data  = static_cast<uint32_t>(rw.data.to_uint());
        bus->write = (rw.kind == uvm::UVM_WRITE);
        return bus;
    }

    void bus2reg(const uvm::uvm_sequence_item* bus_item, uvm::uvm_reg_bus_op& rw) override {
        const auto* bus = dynamic_cast<const apb_subsys_item*>(bus_item);
        if (!bus) return;
        rw.addr   = static_cast<uint32_t>(bus->addr);
        rw.data   = static_cast<uint32_t>(bus->data);
        rw.kind   = static_cast<bool>(bus->write) ? uvm::UVM_WRITE : uvm::UVM_READ;
        rw.status = bus->slverr ? uvm::UVM_NOT_OK : uvm::UVM_IS_OK;
    }
};

class apb_subsys_env : public uvm::uvm_env {
public:
    UVM_COMPONENT_UTILS(apb_subsys_env);

    apb_subsys_sequencer*                      sqr       = nullptr;
    apb_subsys_driver*                         drv       = nullptr;
    apb_subsys_monitor*                        mon       = nullptr;
    apb_subsys_reg_block*                      reg_model = nullptr;
    apb_subsys_reg_adapter*                    adapter   = nullptr;
    uvm::uvm_reg_predictor<apb_subsys_item>*   predictor = nullptr;

    explicit apb_subsys_env(uvm::uvm_component_name name)
      : uvm::uvm_env(name) {}

    void build_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_env::build_phase(phase);
        sqr = apb_subsys_sequencer::type_id::create("sqr", this);
        drv = apb_subsys_driver::type_id::create("drv", this);
        mon = apb_subsys_monitor::type_id::create("mon", this);

        reg_model = apb_subsys_reg_block::type_id::create("reg_model");
        reg_model->build();
        reg_model->reset();

        adapter   = apb_subsys_reg_adapter::type_id::create("adapter");
        predictor = new uvm::uvm_reg_predictor<apb_subsys_item>("predictor");
    }

    void connect_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_env::connect_phase(phase);
        drv->seq_item_port.connect(sqr->seq_item_export);
        reg_model->default_map->set_sequencer(sqr, adapter);
        reg_model->default_map->set_auto_predict(false);
        predictor->map     = reg_model->default_map;
        predictor->adapter = adapter;
        mon->ap.connect(predictor->bus_in);
    }
};

#endif // APB_SUBSYS_ENV_H
