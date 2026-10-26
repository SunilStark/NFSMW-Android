// SPDX-License-Identifier: GPL-3.0-only
// Overlap correction adapted from kayo-b, victorgbd/NFSMW-Recompiled-Mobile 44481d1.
// Matches NFSMW's PPC CRT: forward bytes to aligned destination, four-byte loads/stores, trailing bytes.
// This helper is for the recompiled CRT hook; it does not replace host memcpy globally.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace rex::ppc {
inline void* GuestMemcpy(void* destination, const void* source, size_t count) {
  if (!count || destination == source) return destination;
  const auto d = reinterpret_cast<uintptr_t>(destination);
  const auto s = reinterpret_cast<uintptr_t>(source);
  // Compare distances, not unrelated pointers or potentially overflowing address + count.
  if ((d < s ? s - d : d - s) >= count) return std::memcpy(destination, source, count);
  if (d < s) return std::memmove(destination, source, count);
  volatile uint8_t* out = static_cast<uint8_t*>(destination);
  const volatile uint8_t* in = static_cast<const uint8_t*>(source);
  while (count && (reinterpret_cast<uintptr_t>(out) & 3)) {
    *out++ = *in++;
    --count;
  }
  for (; count >= 4; count -= 4, out += 4, in += 4) {
    const uint8_t a = in[0], b = in[1], c = in[2], e = in[3];
    out[0] = a; out[1] = b; out[2] = c; out[3] = e;
  }
  while (count--) *out++ = *in++;
  return destination;
}
}  // namespace rex::ppc
