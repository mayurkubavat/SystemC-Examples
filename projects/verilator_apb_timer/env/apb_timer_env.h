#ifndef APB_TIMER_ENV_H
#define APB_TIMER_ENV_H

#include <systemc>
#include <uvm>

#include "apb_timer_agent.h"
#include "apb_timer_coverage.h"
#include "apb_timer_ral.h"
#include "apb_timer_scoreboard.h"

class apb_timer_predictor : public uvm::uvm_reg_predictor<apb_timer_item> {
 public:
  explicit apb_timer_predictor(uvm::uvm_component_name name)
      : uvm::uvm_reg_predictor<apb_timer_item>(name) {}

  virtual void write(const apb_timer_item& tr) {
    uvm::uvm_reg_predictor<apb_timer_item>::write(tr);
    if (!tr.error && map != nullptr && map->get_parent() != nullptr)
      map->get_parent()->sample_values();
  }
};

class apb_timer_env : public uvm::uvm_env {
 public:
  UVM_COMPONENT_UTILS(apb_timer_env);

  apb_timer_agent* agt = nullptr;
  apb_timer_scoreboard* scb = nullptr;
  apb_timer_coverage* cov = nullptr;
  apb_timer_reg_block* reg_model = nullptr;
  apb_timer_reg_adapter* adapter = nullptr;
  uvm::uvm_reg_predictor<apb_timer_item>* predictor = nullptr;

  explicit apb_timer_env(uvm::uvm_component_name name) : uvm::uvm_env(name) {}

  virtual void build_phase(uvm::uvm_phase& phase) {
    uvm::uvm_env::build_phase(phase);
    agt = apb_timer_agent::type_id::create("agt", this);
    scb = apb_timer_scoreboard::type_id::create("scb", this);
    cov = apb_timer_coverage::type_id::create("cov", this);

    uvm::uvm_reg::include_coverage("*", uvm::UVM_CVR_ALL);
    reg_model = apb_timer_reg_block::type_id::create("reg_model");
    reg_model->build();
    reg_model->enable_all_coverage();
    reg_model->reset();

    adapter = apb_timer_reg_adapter::type_id::create("adapter");
    predictor = new apb_timer_predictor("predictor");
  }

  virtual void connect_phase(uvm::uvm_phase& phase) {
    uvm::uvm_env::connect_phase(phase);
    agt->mon->ap.connect(cov->analysis_export);

    reg_model->default_map->set_sequencer(agt->sqr, adapter);
    reg_model->default_map->set_auto_predict(false);

    predictor->map = reg_model->default_map;
    predictor->adapter = adapter;
    agt->mon->ap.connect(predictor->bus_in);
  }
};

#endif // APB_TIMER_ENV_H
