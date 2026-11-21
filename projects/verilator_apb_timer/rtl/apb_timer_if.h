#ifndef APB_TIMER_IF_H
#define APB_TIMER_IF_H

#include <cstdint>
#include <systemc>

class apb_timer_if : public sc_core::sc_module {
 public:
  sc_core::sc_signal<bool> PSEL{"PSEL"};
  sc_core::sc_signal<bool> PENABLE{"PENABLE"};
  sc_core::sc_signal<bool> PWRITE{"PWRITE"};
  sc_core::sc_signal<uint32_t> PADDR{"PADDR"};
  sc_core::sc_signal<uint32_t> PWDATA{"PWDATA"};
  sc_core::sc_signal<uint32_t> PRDATA{"PRDATA"};
  sc_core::sc_signal<bool> PREADY{"PREADY"};
  sc_core::sc_signal<bool> PSLVERR{"PSLVERR"};
  sc_core::sc_signal<bool> IRQ{"IRQ"};

  explicit apb_timer_if(const sc_core::sc_module_name& name)
      : sc_core::sc_module(name) {}
};

#endif // APB_TIMER_IF_H
