//
// Created by vladvance on 05.08.2020.
//

#include <random>
#include <vector>
#include <mpl/mpl.hpp>

#include "common.h"
#include "mpi_types.h"


void landlord_routine(const mpl::communicator &comm_world) {

  const int size = comm_world.size();
  const int rank = comm_world.rank();
  const int lamport_clock = 0; //dummy
  std::random_device gen;
  std::uniform_int_distribution<int> rand_hamster_num(1, MAX_HAMSTERS_PER_CONTRACT);
  std::uniform_int_distribution<int> rand_contracts_num(1, size - 1);

  std::vector<struct contract> contracts;
  std::vector<bool> contracts_completed_flags;
  int contracts_num;


  debug("I'm alive!")

  int state = HIRE;

  while (state != FINISH) {
    switch (state) {
      case HIRE: {

        // Num of contracts equal num of gnomes, FOR TESTING PURPOSE
        contracts_num = size - 1;
//                    contracts_num = rand_contracts_num(gen);
        contracts.resize(contracts_num);
        contracts_completed_flags.resize(contracts_num);

        // Generate random contracts
        for (int i = 0; i < contracts_num; ++i) {
          contracts[i].contract_id = i;
          contracts[i].num_hamsters = rand_hamster_num(gen);
          debug("I have new contract: [ ID: %d, NUM_HAMSTERS: %d ]", i, contracts[i].num_hamsters)
        }
        debug("Total num of contracts in this wave: %d", contracts_num)
        debug("There are %d swords and %d poison kits available.", NUM_OF_SWORDS, NUM_OF_POISON_KITS)

        // Send contracts to gnomes
//        comm_world.bcast(LANDLORD_RANK, contracts.begin(), contracts.end());
        debug("Broadcasting contract list.")
        for (int dst_rank = 1; dst_rank < size; ++dst_rank) {
          debug("Sending contract list to GNOME %d.", dst_rank)
          comm_world.send(contracts.begin(), contracts.end(), dst_rank, CONTRACTS);
        }

        state = READ_GANDHI;
//        state = FINISH;
        break;
      }
      case READ_GANDHI: {
        std::vector<contract_completed> completed_contracts(contracts_num);

        for (auto& contract: completed_contracts) {
          const auto& status = comm_world.recv(contract, mpl::any_source, CONTRACT_COMPLETED);
          debug("I was informed that GNOME %d has murdered all %d hamsters and so completed his contract (ID : %d)",
                status.source(),
                contracts[contract.contract_id].num_hamsters,
                contract.contract_id)
          contracts_completed_flags[contract.contract_id] = true;
        }
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