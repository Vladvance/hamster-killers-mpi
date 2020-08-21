#include <mpl/mpl.hpp>

#include "landlord.h"
#include "gnome.h"
#include "common.h"
#include "ascii_art.h"
#include <csignal>
#include <unistd.h>

#define DEBUG

static int rank;
static int lamport_clock;

void signal_callback_handler(int signum) {
  debug("(DEAD): I was wildly killed by unknown force.")
}

int main(int argc, char **argv) {
  const mpl::communicator& comm_world(mpl::environment::comm_world());
  rank = comm_world.rank();
  lamport_clock = 0;

  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);

  if (comm_world.rank() == landlord_rank) { //if landlord
    std::puts(header);
    landlord landlord(mpl::environment::comm_world());
    landlord.run();
  } else {
    usleep(1000);
    gnome gnome(mpl::environment::comm_world());
    gnome.run();
  }

  return EXIT_SUCCESS;
}
