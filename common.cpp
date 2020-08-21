//
// Created by vladvance on 05.08.2020.
//
#include <cstdio>
#include <cstdarg>

void show_message(const int rank, const int lamport_clock, const char* msg, ...) {
  char buf[128];
  std::va_list args;
  va_start (args, msg);
  std::vsprintf(buf, msg, args);
  std::printf("[%8s %d (%d)]: %s\n",(rank == 0) ? "LANDLORD" : "ELF", rank, lamport_clock, buf);
  va_end(args);
}
