#pragma once

#include <arpa/inet.h>
#include <cerrno>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "types.h"

namespace network {

#define MAGIC "1234567890"
#define MAGIC_LEN 11
#define MTU 1500
#define RECV_TIMEOUT_USEC 5

struct icmp_echo {
  // header
  uint8_t type;
  uint8_t code;
  uint16_t checksum;

  uint16_t ident;
  uint16_t seq;

  // data
  double sending_ts;
  char magic[MAGIC_LEN];
};

double get_timestamp();

uint16_t calculate_checksum(unsigned char *buffer, int bytes);

int send_echo_request(int sock, struct sockaddr_in *addr, int ident, int seq);
int ping(host_stat &hstat);

} // namespace network