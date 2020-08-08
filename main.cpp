#include <mpi.h>

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
  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);

  int size, rank_;
  MPI::Init(argc, argv);
  init_types();

  size = MPI::COMM_WORLD.Get_size();
  rank_ = MPI::COMM_WORLD.Get_rank();
  rank = rank_;

  if (rank_ == LANDLORD_RANK) { //if landlord
    landlord_routine(size, rank_);
  } else { //if elf
    elf_routine(size, rank_);
  }

  finalize_types();
  MPI_Finalize();
}
