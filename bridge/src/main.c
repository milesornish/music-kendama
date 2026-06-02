// music-kendama-bridge — SLIP-OSC serial <-> UDP loopback bridge (architecture §5).
//
//   Upstream (Port A): read SLIP frames from the receiver -> UDP 127.0.0.1:9000,
//                      plus a fire-and-forget debug copy to :9500.
//   Downstream (Port B, optional): UDP 127.0.0.1:9001 -> SLIP-encode -> write Port B.
//
// In the two-receiver topology, Port A and Port B are two separate USB devices
// (Receiver-UP and Receiver-DOWN), so each is a device path argument.
//
//   usage: music-kendama-bridge <portA-up> [portB-down]
//
// Build with the Makefile (links libserialport).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <libserialport.h>
#include "slip.h"

#define OSC_PORT   9000   // primary OSC out (to Max for Live)
#define DEBUG_PORT 9500   // passive diagnostic tap
#define DOWN_PORT  9001   // downstream LED commands in

static void die(const char *msg) { fprintf(stderr, "bridge: %s\n", msg); exit(1); }

static struct sp_port *open_port(const char *name, enum sp_mode mode) {
  struct sp_port *p;
  if (sp_get_port_by_name(name, &p) != SP_OK) die("sp_get_port_by_name failed");
  if (sp_open(p, mode) != SP_OK) die("sp_open failed");
  sp_set_baudrate(p, 115200);          // ignored by native USB CDC, set for completeness
  sp_set_bits(p, 8);
  sp_set_parity(p, SP_PARITY_NONE);
  sp_set_stopbits(p, 1);
  sp_set_flowcontrol(p, SP_FLOWCONTROL_NONE);
  return p;
}

static struct sockaddr_in loopback(int port) {
  struct sockaddr_in a;
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons(port);
  return a;
}

// Downstream: UDP :9001 -> SLIP -> Port B.
static void *downstream(void *arg) {
  struct sp_port *portB = (struct sp_port *)arg;
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in a = loopback(DOWN_PORT);
  if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0) die("bind :9001 failed");
  uint8_t in[1024], enc[2048];
  for (;;) {
    ssize_t n = recv(s, in, sizeof(in), 0);
    if (n <= 0) continue;
    size_t e = slip_encode(in, (size_t)n, enc, sizeof(enc));
    if (e) sp_blocking_write(portB, enc, e, 100);
  }
  return NULL;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <portA-up> [portB-down]\n", argv[0]);
    return 1;
  }

  int udp = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp < 0) die("socket failed");
  struct sockaddr_in to_osc = loopback(OSC_PORT);
  struct sockaddr_in to_dbg = loopback(DEBUG_PORT);

  struct sp_port *portA = open_port(argv[1], SP_MODE_READ);
  printf("bridge: Port A (upstream) %s -> UDP :%d (+debug :%d)\n", argv[1], OSC_PORT, DEBUG_PORT);

  if (argc >= 3) {
    struct sp_port *portB = open_port(argv[2], SP_MODE_WRITE);
    pthread_t t;
    pthread_create(&t, NULL, downstream, portB);
    printf("bridge: Port B (downstream) %s <- UDP :%d\n", argv[2], DOWN_PORT);
  }

  uint8_t framebuf[1024];
  slip_decoder_t dec;
  slip_decoder_init(&dec, framebuf, sizeof(framebuf));
  uint8_t rb[256];
  unsigned long frames = 0;

  for (;;) {
    int n = sp_blocking_read(portA, rb, sizeof(rb), 50);
    if (n < 0) die("serial read error");
    for (int i = 0; i < n; i++) {
      size_t flen = slip_decode_byte(&dec, rb[i]);
      if (flen) {
        sendto(udp, framebuf, flen, 0, (struct sockaddr *)&to_osc, sizeof(to_osc));
        sendto(udp, framebuf, flen, 0, (struct sockaddr *)&to_dbg, sizeof(to_dbg));
        if (++frames % 50 == 0) printf("bridge: forwarded %lu OSC frames\n", frames);
      }
    }
  }
  return 0;
}
