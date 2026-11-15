// ----------------------------------------------------------------------------
// bus_coverage — FC4SC functional coverage collected by a UVM subscriber
//
// FC4SC (Functional Coverage for SystemC) mirrors the SV covergroup syntax:
//
//   SV:  covergroup bus_cg;
//          cmd_cp : coverpoint cmd { bins read = {0}; bins write = {1}; }
//          len_cp  : coverpoint len  { bins single = {1}; bins burst[4] = {[2:16]}; }
//          cmd_x_len : cross cmd_cp, len_cp;
//        endgroup
//
//   SC:  class bus_cg : public covergroup {
//          COVERPOINT(int, cmd_cp, cmd) { bin<int>("read", 0), bin<int>("write", 1) };
//          ...
//          cross<int, int> cmd_x_len = cross<int, int>(this, "cmd_x_len", &cmd_cp, &len_cp);
//        };
//
// Every covergroup registers itself with FC4SC's global database, and
// xml_printer::coverage_save() writes that database as UCIS XML, the
// Accellera interchange format other coverage tools can read and merge.
// ----------------------------------------------------------------------------
#ifndef BUS_COVERAGE_H
#define BUS_COVERAGE_H

#include <systemc>
#include <uvm>

#include <sstream>

#include "fc4sc.hpp"
#include "bus_item.h"

// The COVERPOINT macro expands to unqualified FC4SC names (coverpoint<T>),
// so the FC4SC examples bring the namespace in.
using namespace fc4sc;

class bus_cg : public covergroup {
public:
  int cmd = 0;
  int addr = 0;
  int len = 0;

  CG_CONS(bus_cg) {}

  void sample(const bus_item& item) {
    cmd = item.cmd;
    addr = item.addr;
    len = item.len;
    covergroup::sample();
  }

  COVERPOINT(int, cmd_cp, cmd) {
    bin<int>("read", BUS_READ),
    bin<int>("write", BUS_WRITE)
  };

  // Address map regions
  COVERPOINT(int, addr_cp, addr) {
    bin<int>("regs", interval(0, 15)),
    bin<int>("fifo", interval(16, 127)),
    bin<int>("mem", interval(128, 255))
  };

  // Single beat, plus bursts split into 4 equal bins: [2:5] [6:9] [10:13] [14:16]
  COVERPOINT(int, len_cp, len) {
    bin<int>("single", 1),
    bin_array<int>("burst", 4, interval(2, 16)),
    illegal_bin<int>("zero", 0)
  };

  // Every access type with every burst size
  cross<int, int> cmd_x_len = cross<int, int>(this, "cmd_x_len", &cmd_cp, &len_cp);
};

class bus_coverage : public uvm::uvm_subscriber<bus_item> {
public:
  UVM_COMPONENT_UTILS(bus_coverage);

  bus_cg cg;
  double goal = 100.0;  // percent

  bus_coverage(uvm::uvm_component_name name)
    : uvm::uvm_subscriber<bus_item>(name), cg("bus_cg") {}

  // Called for every item published on the analysis port
  virtual void write(const bus_item& item) { cg.sample(item); }

  void report_phase(uvm::uvm_phase& phase) {
    std::ostringstream ss;
    ss << "cmd_cp=" << cg.cmd_cp.get_inst_coverage() << "%"
       << " addr_cp=" << cg.addr_cp.get_inst_coverage() << "%"
       << " len_cp=" << cg.len_cp.get_inst_coverage() << "%"
       << " cmd_x_len=" << cg.cmd_x_len.get_inst_coverage() << "%"
       << " total=" << cg.get_inst_coverage() << "%";
    UVM_INFO(get_name(), ss.str(), uvm::UVM_LOW);

    if (cg.get_inst_coverage() < goal) {
      UVM_ERROR(get_name(), "coverage goal not met");
    }

    // UCIS XML report of every registered covergroup
    xml_printer::coverage_save("fc4sc_cov_results.xml");
    UVM_INFO(get_name(), "wrote fc4sc_cov_results.xml", uvm::UVM_LOW);
  }
};

#endif // BUS_COVERAGE_H
