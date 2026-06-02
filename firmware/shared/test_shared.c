// test_shared.c — host-side unit test for the shared OSC + SLIP codecs.
// Build/run:  cc firmware/shared/test_shared.c -o /tmp/test_shared && /tmp/test_shared
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "osc.h"
#include "slip.h"

static void hexdump(const char *label, const uint8_t *b, size_t n) {
  printf("%s (%zu):", label, n);
  for (size_t i = 0; i < n; i++) printf(" %02X", b[i]);
  printf("\n");
}

int main(void) {
  // 1. Build an OSC message: /kendama/tama/accel ,fff
  uint8_t osc_buf[128];
  osc_t m; osc_init(&m, osc_buf, sizeof(osc_buf));
  osc_begin(&m, "/kendama/tama/accel", "fff");
  osc_add_float(&m, 1.0f); osc_add_float(&m, -9.81f); osc_add_float(&m, 0.5f);
  assert(osc_ok(&m));
  assert(m.len % 4 == 0);   // OSC packets must be 4-byte aligned
  hexdump("OSC", osc_buf, m.len);
  printf("  -> length %zu, 4-byte aligned: %s\n", m.len, (m.len % 4 == 0) ? "yes" : "NO");

  // 2. SLIP round-trip with bytes that MUST be escaped (END, ESC)
  uint8_t payload[4] = {0x01, SLIP_END, SLIP_ESC, 0x02};
  uint8_t enc[32];
  size_t enclen = slip_encode(payload, sizeof(payload), enc, sizeof(enc));
  hexdump("SLIP-encoded (w/ escapes)", enc, enclen);
  uint8_t dec[32]; slip_decoder_t d; slip_decoder_init(&d, dec, sizeof(dec));
  size_t framelen = 0;
  for (size_t i = 0; i < enclen; i++) { size_t r = slip_decode_byte(&d, enc[i]); if (r) framelen = r; }
  assert(framelen == sizeof(payload) && memcmp(payload, dec, framelen) == 0);
  printf("  -> SLIP round-trip with escaping: OK\n");

  // 3. OSC-over-SLIP round-trip (the real path)
  uint8_t enc2[300]; size_t e2 = slip_encode(osc_buf, m.len, enc2, sizeof(enc2));
  uint8_t dec2[300]; slip_decoder_t d2; slip_decoder_init(&d2, dec2, sizeof(dec2));
  size_t f2 = 0;
  for (size_t i = 0; i < e2; i++) { size_t r = slip_decode_byte(&d2, enc2[i]); if (r) f2 = r; }
  assert(f2 == m.len && memcmp(osc_buf, dec2, f2) == 0);
  printf("  -> OSC-over-SLIP round-trip: OK (%zu bytes)\n", f2);

  // 4. OSC round-trip: build an int LED command, parse it back
  uint8_t led[64];
  osc_t lm; osc_init(&lm, led, sizeof(led));
  osc_begin(&lm, "/kendama/tama/led/rgb", "iii");
  osc_add_int(&lm, 255); osc_add_int(&lm, 0); osc_add_int(&lm, 128);
  assert(osc_ok(&lm));
  char addr[64]; osc_reader_t r;
  assert(osc_parse(led, lm.len, addr, sizeof(addr), &r));
  int32_t rr, gg, bb;
  assert(osc_read_int(&r, &rr) && osc_read_int(&r, &gg) && osc_read_int(&r, &bb));
  printf("OSC parse: addr='%s' rgb=(%d,%d,%d)\n", addr, rr, gg, bb);
  assert(strcmp(addr, "/kendama/tama/led/rgb") == 0 && rr == 255 && gg == 0 && bb == 128);
  printf("  -> OSC build+parse round-trip: OK\n");

  // also parse back the float accel message from step 1
  osc_reader_t r2; char a2[64];
  assert(osc_parse(osc_buf, m.len, a2, sizeof(a2), &r2));
  float fx, fy, fz;
  assert(osc_read_float(&r2, &fx) && osc_read_float(&r2, &fy) && osc_read_float(&r2, &fz));
  printf("OSC parse: addr='%s' fff=(%.2f,%.2f,%.2f)\n", a2, fx, fy, fz);
  assert(strcmp(a2, "/kendama/tama/accel") == 0 && fx == 1.0f && fz == 0.5f);
  printf("  -> OSC float parse: OK\n");

  printf("\nALL SHARED-LAYER TESTS PASSED\n");
  return 0;
}
