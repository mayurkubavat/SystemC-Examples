#ifndef APB_TIMER_RAL_H
#define APB_TIMER_RAL_H

#include <cstdint>
#include <string>

#include <fc4sc.hpp>
#include <systemc>
#include <uvm>
#include "uvmsc/reg/uvm_fc4sc_reg_cvr.h"

#include "apb_timer_item.h"

// 0x00 CTRL: [0] en, [1] periodic, [2] irq_en, [5:4] prescale
class timer_ctrl_reg : public uvm::uvm_fc4sc_reg {
 public:
  uvm::uvm_reg_field* en = nullptr;
  uvm::uvm_reg_field* periodic = nullptr;
  uvm::uvm_reg_field* irq_en = nullptr;
  uvm::uvm_reg_field* prescale = nullptr;

  UVM_OBJECT_UTILS(timer_ctrl_reg);

  explicit timer_ctrl_reg(const std::string& name = "timer_ctrl_reg")
      : uvm::uvm_fc4sc_reg(name, 8, uvm::UVM_CVR_REG_BITS | uvm::UVM_CVR_FIELD_VALS) {}

  void build() {
    en = uvm::uvm_reg_field::type_id::create("en");
    en->configure(this, 1, 0, "RW", true, 0x0, true, true, false);

    periodic = uvm::uvm_reg_field::type_id::create("periodic");
    periodic->configure(this, 1, 1, "RW", false, 0x0, true, true, false);

    irq_en = uvm::uvm_reg_field::type_id::create("irq_en");
    irq_en->configure(this, 1, 2, "RW", false, 0x0, true, true, false);

    prescale = uvm::uvm_reg_field::type_id::create("prescale");
    prescale->configure(this, 2, 4, "RW", false, 0x0, true, true, false);
  }
};

// 0x04 LOAD: [7:0] reload value (reset 0x0F)
class timer_load_reg : public uvm::uvm_fc4sc_reg {
 public:
  uvm::uvm_reg_field* load = nullptr;

  UVM_OBJECT_UTILS(timer_load_reg);

  explicit timer_load_reg(const std::string& name = "timer_load_reg")
      : uvm::uvm_fc4sc_reg(name, 8, uvm::UVM_CVR_FIELD_VALS) {}

  void build() {
    load = uvm::uvm_reg_field::type_id::create("load");
    load->configure(this, 8, 0, "RW", false, 0x0F, true, true, true);
  }
};

// 0x08 COUNT: [7:0] live counter (RO, volatile)
class timer_count_reg : public uvm::uvm_fc4sc_reg {
 public:
  uvm::uvm_reg_field* count = nullptr;

  UVM_OBJECT_UTILS(timer_count_reg);

  explicit timer_count_reg(const std::string& name = "timer_count_reg")
      : uvm::uvm_fc4sc_reg(name, 8, uvm::UVM_CVR_FIELD_VALS) {}

  void build() {
    count = uvm::uvm_reg_field::type_id::create("count");
    count->configure(this, 8, 0, "RO", true, 0x0F, true, false, true);
  }
};

// 0x0C IRQ_STATUS: [0] irq_pending (W1C, volatile)
class timer_irq_status_reg : public uvm::uvm_fc4sc_reg {
 public:
  uvm::uvm_reg_field* irq_pending = nullptr;

  UVM_OBJECT_UTILS(timer_irq_status_reg);

  explicit timer_irq_status_reg(const std::string& name = "timer_irq_status_reg")
      : uvm::uvm_fc4sc_reg(name, 8, uvm::UVM_CVR_FIELD_VALS) {}

  void build() {
    irq_pending = uvm::uvm_reg_field::type_id::create("irq_pending");
    irq_pending->configure(this, 1, 0, "W1C", true, 0x0, true, true, true);
  }
};

// 0x10 SCRATCH: [7:0] general-purpose RW register (bit-level toggle + values)
class timer_scratch_reg : public uvm::uvm_fc4sc_reg {
 public:
  uvm::uvm_reg_field* data = nullptr;

  UVM_OBJECT_UTILS(timer_scratch_reg);

  explicit timer_scratch_reg(const std::string& name = "timer_scratch_reg")
      : uvm::uvm_fc4sc_reg(name, 8, uvm::UVM_CVR_REG_BITS | uvm::UVM_CVR_FIELD_VALS) {}

  void build() {
    data = uvm::uvm_reg_field::type_id::create("data");
    data->configure(this, 8, 0, "RW", false, 0x00, true, true, true);
  }
};

class apb_timer_reg_block : public uvm::uvm_fc4sc_reg_block {
 public:
  timer_ctrl_reg* ctrl = nullptr;
  timer_load_reg* load = nullptr;
  timer_count_reg* count = nullptr;
  timer_irq_status_reg* irq_status = nullptr;
  timer_scratch_reg* scratch = nullptr;

  UVM_OBJECT_UTILS(apb_timer_reg_block);

  explicit apb_timer_reg_block(const std::string& name = "apb_timer_reg_block")
      : uvm::uvm_fc4sc_reg_block(name, uvm::UVM_CVR_ALL) {}

  void build() {
    default_map = create_map("default_map", 0x0, 4, uvm::UVM_LITTLE_ENDIAN);

    ctrl = timer_ctrl_reg::type_id::create("ctrl");
    ctrl->configure(this, nullptr);
    ctrl->build();
    default_map->add_reg(ctrl, 0x00, "RW");

    load = timer_load_reg::type_id::create("load");
    load->configure(this, nullptr);
    load->build();
    default_map->add_reg(load, 0x04, "RW");

    count = timer_count_reg::type_id::create("count");
    count->configure(this, nullptr);
    count->build();
    default_map->add_reg(count, 0x08, "RO");

    irq_status = timer_irq_status_reg::type_id::create("irq_status");
    irq_status->configure(this, nullptr);
    irq_status->build();
    default_map->add_reg(irq_status, 0x0C, "RW");

    scratch = timer_scratch_reg::type_id::create("scratch");
    scratch->configure(this, nullptr);
    scratch->build();
    default_map->add_reg(scratch, 0x10, "RW");

    lock_model();
  }
};

class apb_timer_reg_adapter : public uvm::uvm_reg_adapter {
 public:
  UVM_OBJECT_UTILS(apb_timer_reg_adapter);

  explicit apb_timer_reg_adapter(const std::string& name = "apb_timer_reg_adapter")
      : uvm::uvm_reg_adapter(name) {
    supports_byte_enable = false;
    provides_responses = true;
  }

  virtual uvm::uvm_sequence_item* reg2bus(const uvm::uvm_reg_bus_op& rw) {
    auto* item = apb_timer_item::type_id::create("bus_item");
    item->addr = static_cast<uint32_t>(rw.addr.to_uint64());
    item->data = static_cast<uint32_t>(rw.data.to_uint64());
    item->write = (rw.kind == uvm::UVM_WRITE);
    return item;
  }

  virtual void bus2reg(const uvm::uvm_sequence_item* bus_item,
                       uvm::uvm_reg_bus_op& rw) {
    const auto* item = dynamic_cast<const apb_timer_item*>(bus_item);
    if (!item) UVM_FATAL("BUS2REG", "Expected apb_timer_item");
    rw.addr = static_cast<uint32_t>(item->addr);
    rw.data = static_cast<uint32_t>(item->data);
    rw.kind = static_cast<bool>(item->write) ? uvm::UVM_WRITE : uvm::UVM_READ;
    rw.status = item->error ? uvm::UVM_NOT_OK : uvm::UVM_IS_OK;
  }
};

#endif // APB_TIMER_RAL_H
