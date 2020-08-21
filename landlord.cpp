#include <random>
#include <mpl/mpl.hpp>

#include "common.h"
#include "mpi_types.h"
#include "landlord.h"


void landlord::run() {

  debug("I'm alive!")

  int state = HIRE;

  while (state != FINISH) {
    switch (state) {
      case HIRE: {
        std::uniform_int_distribution<int> rand_hamster_num(min_hamsters_per_contract, max_hamsters_per_contract);
        std::uniform_int_distribution<int> rand_contracts_num(1, gnomes_num);

        // Num of contracts equal num of gnomes, FOR TESTING PURPOSE
        contracts_num = gnomes_num;
//        contracts_num = rand_contracts_num(gen);
        contracts.resize(contracts_num);
        is_completed.resize(contracts_num);

        // Generate random contracts
        for (int i = 0; i < contracts_num; ++i) {
          contracts[i].contract_id = i;
          contracts[i].num_hamsters = rand_hamster_num(gen);
          debug("I have new contract: [ ID: %d, NUM_HAMSTERS: %d ]", i, contracts[i].num_hamsters)
        }
        debug("Total num of contracts in this wave: %d", contracts_num)
        debug("There are %d swords and %d poison kits available.", NUM_OF_SWORDS, NUM_OF_POISON_KITS)

        // Send contracts to gnomes
        debug("Broadcasting contract list.")
        for (int dst_rank = gnome_first; dst_rank < size; ++dst_rank) {
//          debug("Sending contract list to GNOME %d.", dst_rank)
          comm_world.send(contracts.begin(), contracts.end(), dst_rank, CONTRACTS);
        }

        state = READ_GANDHI;
//        state = FINISH;
        break;
      }
      case READ_GANDHI: {

        receive_all_cc();
        state = FINISH;
        break;
      }
      default: {
        //should never reach here
        debug("Entered superposition state. Committing suicide.")
        return;
      }
    }
  }
  debug("My mission in this world completed. Committing suicide.")
}
void landlord::receive_all_cc() {
  struct contract_completed cc_msg{};
  for (int i = 0; i < contracts_num; ++i) {
    const auto& status = comm_world.recv(cc_msg, mpl::any_source, CONTRACT_COMPLETED);
    debug("I was informed that GNOME %d has murdered all %d hamsters and so completed his contract (ID : %d)",
          status.source(),
          contracts[cc_msg.contract_id].num_hamsters,
          cc_msg.contract_id)
    is_completed[cc_msg.contract_id] = true;
  }
}
