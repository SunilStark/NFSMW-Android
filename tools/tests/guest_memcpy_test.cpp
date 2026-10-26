// SPDX-License-Identifier: GPL-3.0-only
// Independent model of the PPC code verified in the user's PAL Spain XEX at 0x826BE1B0.
#include <rex/ppc/guest_memcpy.h>
#include <array>
#include <cassert>
#include <cstdio>

static void Model(uint8_t* out, const uint8_t* in, size_t size) {
  size_t byte = 0;
  const size_t alignment = (4 - (reinterpret_cast<uintptr_t>(out) & 3)) & 3;
  for (; byte < size && byte < alignment; ++byte) out[byte] = in[byte];
  const size_t end = byte + ((size - byte) / 4) * 4;
  for (; byte < end; byte += 4) {
    uint32_t word;
    std::memcpy(&word, in + byte, 4);
    std::memcpy(out + byte, &word, 4);
  }
  for (; byte < size; ++byte) out[byte] = in[byte];
}

int main() {
  unsigned cases = 0;
  for (unsigned destination = 0; destination < 64; ++destination) {
    for (unsigned source = 0; source < 64; ++source) {
      for (unsigned size = 0; size < 200; ++size) {
        alignas(16) std::array<uint8_t, 272> expected, actual;
        for (unsigned i = 0; i < expected.size(); ++i) expected[i] = uint8_t((i * 37) ^ (i >> 3));
        actual = expected;
        Model(expected.data() + destination, expected.data() + source, size);
        assert(rex::ppc::GuestMemcpy(actual.data() + destination, actual.data() + source, size) == actual.data() + destination);
        assert(actual == expected);
        ++cases;
      }
    }
  }
  // Exact pattern used to remove an animation controller from the active pointer array.
  std::array<uint32_t, 64> controllers;
  for (unsigned removed = 0; removed < controllers.size(); ++removed) {
    for (unsigned i = 0; i < controllers.size(); ++i) controllers[i] = 0x82000000 + i * 16;
    rex::ppc::GuestMemcpy(controllers.data() + removed, controllers.data() + removed + 1,
                         (controllers.size() - removed - 1) * sizeof(uint32_t));
    for (unsigned i = removed; i + 1 < controllers.size(); ++i) assert(controllers[i] == 0x82000000 + (i + 1) * 16);
  }
  std::printf("Guest memcpy: %u overlap/alignment/size cases and 64 controller removals passed.\n", cases);
}
