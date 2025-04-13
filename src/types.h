#pragma once

#include <string>

struct host_stat {
  std::string ip;
  int times;  // общее число попыток
  int stimes; // число успешных попыток
};