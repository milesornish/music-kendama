// osc.h — minimal OSC 1.0 message builder, header-only, C-compatible.
// Enough to emit sensor/gesture messages: address + ,f.../,i typetags, big-endian,
// 4-byte aligned. Used by the receiver firmware to wrap ESP-NOW sensor data as OSC.
#ifndef KENDAMA_OSC_H
#define KENDAMA_OSC_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef struct { uint8_t *buf; size_t cap; size_t len; int ok; } osc_t;

static inline void osc_init(osc_t *m, uint8_t *buf, size_t cap) {
  m->buf = buf; m->cap = cap; m->len = 0; m->ok = 1;
}

// Write a null-terminated string padded to a 4-byte boundary.
static inline void osc__str(osc_t *m, const char *s) {
  size_t l = strlen(s) + 1;
  size_t pad = (4 - (l & 3)) & 3;
  if (m->len + l + pad > m->cap) { m->ok = 0; return; }
  memcpy(m->buf + m->len, s, l);
  m->len += l;
  while (pad--) m->buf[m->len++] = 0;
}

static inline void osc__be32(osc_t *m, uint32_t v) {
  if (m->len + 4 > m->cap) { m->ok = 0; return; }
  m->buf[m->len++] = (uint8_t)(v >> 24);
  m->buf[m->len++] = (uint8_t)(v >> 16);
  m->buf[m->len++] = (uint8_t)(v >> 8);
  m->buf[m->len++] = (uint8_t)(v);
}

// Begin a message: address + typetag string (the leading ',' is added here, so pass
// e.g. "fff" for three floats, "sf" for string+float).
static inline void osc_begin(osc_t *m, const char *address, const char *typetags) {
  m->len = 0; m->ok = 1;
  osc__str(m, address);
  char tt[16];
  tt[0] = ',';
  size_t k = strlen(typetags);
  if (k > sizeof(tt) - 2) k = sizeof(tt) - 2;
  memcpy(tt + 1, typetags, k);
  tt[1 + k] = 0;
  osc__str(m, tt);
}

static inline void osc_add_float(osc_t *m, float f) {
  uint32_t v; memcpy(&v, &f, 4); osc__be32(m, v);
}
static inline void osc_add_int(osc_t *m, int32_t i) { osc__be32(m, (uint32_t)i); }

// Append a 4-byte-aligned OSC string argument (typetag 's').
static inline void osc_add_str(osc_t *m, const char *s) { osc__str(m, s); }

// 1 if the message built without overflow, else 0.
static inline int osc_ok(const osc_t *m) { return m->ok; }

// --- Parsing -----------------------------------------------------------------
// Minimal reader for incoming OSC messages (e.g. LED commands from Max for Live).

typedef struct {
  const uint8_t *buf;
  size_t len;
  size_t pos;          // cursor into the argument section
  const char *typetags; // points into buf, just past the leading ','
} osc_reader_t;

static inline size_t osc__padded(size_t n) { return (n + 3) & ~((size_t)3); }

// Parse address + typetags. Returns 1 on success, filling `addr` and `r`.
static inline int osc_parse(const uint8_t *buf, size_t len, char *addr, size_t addr_cap,
                            osc_reader_t *r) {
  size_t z = 0;
  while (z < len && buf[z] != 0) z++;
  if (z >= len || z + 1 > addr_cap) return 0;
  memcpy(addr, buf, z);
  addr[z] = 0;
  size_t p = osc__padded(z + 1);
  if (p >= len || buf[p] != ',') return 0;
  size_t tz = p + 1;
  while (tz < len && buf[tz] != 0) tz++;
  if (tz >= len) return 0;
  r->buf = buf;
  r->len = len;
  r->typetags = (const char *)(buf + p + 1);   // null-terminated within buf
  r->pos = osc__padded((tz - p) + 1) + p;       // args start after padded typetag block
  return 1;
}

static inline int osc_read_int(osc_reader_t *r, int32_t *out) {
  if (r->pos + 4 > r->len) return 0;
  const uint8_t *b = r->buf + r->pos;
  *out = (int32_t)(((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
                   ((uint32_t)b[2] << 8) | (uint32_t)b[3]);
  r->pos += 4;
  return 1;
}

static inline int osc_read_float(osc_reader_t *r, float *out) {
  int32_t v;
  if (!osc_read_int(r, &v)) return 0;
  memcpy(out, &v, 4);
  return 1;
}

#endif // KENDAMA_OSC_H
