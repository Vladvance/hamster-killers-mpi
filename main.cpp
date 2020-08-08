#include <mpi.h>
#include <mpl/mpl.hpp>

#include "landlord_routine.h"
#include "elf_routine.h"
#include "mpi_types.h"
#include "common.h"
#include <csignal>

#define DEBUG

static int rank;

void signal_callback_handler(int signum) {
  debug("(DEAD): I was wildly killed by unknown force.")
}

int main(int argc, char **argv) {
  std::ios::sync_with_stdio(false);

  const mpl::communicator& comm_world(mpl::environment::comm_world());
  rank = comm_world.rank();

  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);

//  init_types();

  testy str(13,13.0);
  if (comm_world.rank() == LANDLORD_RANK) { //if landlord
//    landlord_routine(comm_world.size(), comm_world.rank());
    std::printf("Sending testy....\n");
    comm_world.send(str, 1);

  } else { //if elf
//    elf_routine(comm_world.size(), comm_world.rank());
    comm_world.recv(str, 0);
    std::cout << "Received testy: " << str << "\n";
  }

//  finalize_types();
//  MPI_Finalize();
  return EXIT_SUCCESS;
}
