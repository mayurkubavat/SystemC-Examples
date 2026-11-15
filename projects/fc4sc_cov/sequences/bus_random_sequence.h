// ----------------------------------------------------------------------------
// bus_random_sequence — uniform random reads and writes
//
// The seed comes from config_db ("seed"), so a run can be reproduced:
//   ./fc4sc_cov_sim 1234
// ----------------------------------------------------------------------------
#ifndef BUS_RANDOM_SEQUENCE_H
#define BUS_RANDOM_SEQUENCE_H

#include <systemc>
#include <uvm>

#include <random>

#include "bus_item.h"

class bus_random_sequence : public uvm::uvm_sequence<bus_item> {
public:
  unsigned num_items = 300;

  UVM_OBJECT_UTILS(bus_random_sequence);

  bus_random_sequence(const std::string& name = "bus_random_sequence")
    : uvm::uvm_sequence<bus_item>(name) {}

  void body() {
    unsigned seed = 1;
    uvm::uvm_config_db<unsigned>::get(nullptr, "", "seed", seed);
    uvm::uvm_config_db<unsigned>::get(nullptr, "", "num_items", num_items);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> cmd_d(BUS_READ, BUS_WRITE);
    std::uniform_int_distribution<int> addr_d(0, 255);
    std::uniform_int_distribution<int> len_d(1, 16);

    for (unsigned i = 0; i < num_items; ++i) {
      bus_item* item = bus_item::type_id::create("item");
      start_item(item);
      item->cmd = cmd_d(rng);
      item->addr = addr_d(rng);
      item->len = len_d(rng);
      finish_item(item);
    }
  }
};

#endif // BUS_RANDOM_SEQUENCE_H
