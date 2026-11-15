// ----------------------------------------------------------------------------
// bus_driver — consumes bus items and publishes them for coverage
//
// No pin-level DUT: the driver spends one cycle per beat and writes each
// item to an analysis port, standing in for a monitor. In a real bench the
// coverage subscriber connects to the monitor's analysis port instead.
// ----------------------------------------------------------------------------
#ifndef BUS_DRIVER_H
#define BUS_DRIVER_H

#include <systemc>
#include <uvm>

#include "bus_item.h"

class bus_driver : public uvm::uvm_driver<bus_item> {
public:
  UVM_COMPONENT_UTILS(bus_driver);

  uvm::uvm_analysis_port<bus_item> ap;

  bus_driver(uvm::uvm_component_name name)
    : uvm::uvm_driver<bus_item>(name), ap("ap") {}

  void run_phase(uvm::uvm_phase& phase) {
    while (true) {
      bus_item item;
      seq_item_port->get_next_item(item);
      sc_core::wait(10 * item.len, sc_core::SC_NS);
      UVM_INFO(get_name(), item.convert2string(), uvm::UVM_HIGH);
      ap.write(item);
      seq_item_port->item_done();
    }
  }
};

#endif // BUS_DRIVER_H
