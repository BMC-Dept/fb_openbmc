#ifndef _BIC_CPLD_H_
#define _BIC_CPLD_H_
#include <string>
#include "fw-util.h"

class CpldComponent : public Component {
  uint8_t slot_id = 0;
  uint8_t bus = 0;
  uint8_t addr = 0;
  uint8_t intf = 0;
  public:
    CpldComponent(std::string fru, std::string board, std::string comp, uint8_t _slot_id, uint8_t _bus, uint8_t _addr, uint8_t _intf)
      : Component(fru, board, comp), slot_id(_slot_id), bus(_bus), addr(_addr), intf(_intf){}
    int print_version();
};

#endif
