#include "LoRaProvider.h"

namespace dirk {

// Keep the IRQ flag in internal DRAM and define the ISR out-of-line.
// This mirrors RadioLib's ESP32 interrupt pattern while avoiding the
// ESP32-S3 linker "dangerous relocation" produced by the previous
// inline class ISR/literal placement.
DRAM_ATTR volatile bool LoRaProvider::rxFlag_ = false;

void IRAM_ATTR LoRaProvider::rxThunk() {
  rxFlag_ = true;
}

} // namespace dirk
