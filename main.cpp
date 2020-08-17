#include <mpl/mpl.hpp>

#include "landlord_routine.h"
#include "gnome_routine.h"
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

  if(rank==LANDLORD_RANK)
    std::puts(header);
  else
    usleep(1000);

  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);


  if (comm_world.rank() == LANDLORD_RANK) { //if landlord
    landlord_routine(comm_world);
  } else { //if gnome
    gnome_routine(comm_world);
  }

  return EXIT_SUCCESS;
}
