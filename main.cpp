#include <unistd.h>

#include <csignal>
#include <mpl/mpl.hpp>

#include "arg_parser.h"
#include "ascii_art.h"
#include "gnome.h"
#include "landlord.h"

#define DEBUG

void signal_callback_handler(int signum) {
  printf("[Rank: %d] (DEAD): I was wildly killed by unknown force.\n",
         mpl::environment::comm_world().rank());
}

int main(int argc, char **argv) {
  auto config = ArgParser::parse(argc, argv);
  Landlord::minHamstersPerContract = config.minHamstersPerContract;
  Landlord::maxHamstersPerContract = config.maxHamstersPerContract;
  Gnome::swordsTotal = config.swordsTotal;
  Gnome::poisonTotal = config.poisonTotal;

  const mpl::communicator &comm_world(mpl::environment::comm_world());

  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);

  if (comm_world.rank() == Landlord::landlordRank) {
    std::puts(header);
    printf("There are %d swords and %d poison kits available.\n",
           Gnome::swordsTotal, Gnome::poisonTotal);
    Landlord landlord(comm_world);
    landlord.run(config.maxRounds);
  } else {
    usleep(1000);
    Gnome gnome(comm_world);
    gnome.run(config.maxRounds);
  }

  return EXIT_SUCCESS;
}
