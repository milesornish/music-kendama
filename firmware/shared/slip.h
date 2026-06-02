// slip.h — SLIP framing (RFC 1055), header-only, C-compatible.
// Shared by the receiver firmware (encode) and the C bridge (decode).
#ifndef KENDAMA_SLIP_H
#define KENDAMA_SLIP_H

#include <stdint.h>
#include <stddef.h>

#define SLIP_END     0xC0
#define SLIP_ESC     0xDB
#define SLIP_ESC_END 0xDC
#define SLIP_ESC_ESC 0xDD

// Encode `n` bytes of `src` into `dst` as one SLIP frame (END-delimited both ends).
// Returns the encoded length, or 0 if `dst` (cap bytes) is too small. Worst case: 2*n + 2.
static inline size_t slip_encode(const uint8_t *src, size_t n, uint8_t *dst, size_t cap) {
  size_t o = 0;
  if (cap < 2) return 0;
  dst[o++] = SLIP_END;
  for (size_t i = 0; i < n; i++) {
    uint8_t b = src[i];
    if (b == SLIP_END)      { if (o + 2 > cap) return 0; dst[o++] = SLIP_ESC; dst[o++] = SLIP_ESC_END; }
    else if (b == SLIP_ESC) { if (o + 2 > cap) return 0; dst[o++] = SLIP_ESC; dst[o++] = SLIP_ESC_ESC; }
    else                    { if (o + 1 > cap) return 0; dst[o++] = b; }
  }
  if (o + 1 > cap) return 0;
  dst[o++] = SLIP_END;
  return o;
}

// Incremental decoder: feed bytes one at a time. Returns a frame's length (>0) when a
// complete, non-empty frame has been assembled in `buf`; otherwise 0. Empty frames
// (back-to-back END bytes) return 0 and are skipped.
typedef struct { uint8_t *buf; size_t cap; size_t len; int esc; } slip_decoder_t;

static inline void slip_decoder_init(slip_decoder_t *d, uint8_t *buf, size_t cap) {
  d->buf = buf; d->cap = cap; d->len = 0; d->esc = 0;
}

static inline size_t slip_decode_byte(slip_decoder_t *d, uint8_t b) {
  if (b == SLIP_END) {
    size_t n = d->len;
    d->len = 0; d->esc = 0;
    return n;                      // 0 for empty frame; caller ignores
  }
  if (d->esc) {
    d->esc = 0;
    if (b == SLIP_ESC_END) b = SLIP_END;
    else if (b == SLIP_ESC_ESC) b = SLIP_ESC;
  } else if (b == SLIP_ESC) {
    d->esc = 1;
    return 0;
  }
  if (d->len < d->cap) d->buf[d->len++] = b;   // silently drop on overflow
  return 0;
}

#endif // KENDAMA_SLIP_H
