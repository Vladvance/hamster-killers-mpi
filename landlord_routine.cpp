//
// Created by vladvance on 05.08.2020.
//

#include <random>
#include <vector>
#include <mpi.h>

#include "common.h"
#include "mpi_types.h"


void landlord_routine(int size, int rank) {
  debug("I'm alive!")

  std::random_device gen;
  std::uniform_int_distribution<int> rand_hamster_num(1, MAX_HAMSTERS_IN_CONTRACTS);
  std::uniform_int_distribution<int> rand_contracts_num(1, size - 1);
  MPI_Status status;


  std::vector<struct contract> contracts;
  std::vector<bool> contracts_completed_flags;
  int contracts_num;

  int state = HIRE;

  while (state != FINISH) {
    switch (state) {
      case HIRE: {

        // Num of contracts equal num of elves, FOR TESTING PURPOSE
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

        // Send contracts to elves
        for (int dst_rank = 1; dst_rank < size; ++dst_rank) {
          debug("Sending contract list to ELF %d.", dst_rank)
          MPI_Send(contracts.data(), contracts_num, MPI_CONTRACT_T, dst_rank, CONTRACTS, MPI::COMM_WORLD);
        }

//        state = READ_GANDHI;
        state = FINISH;
        break;
      }
      case READ_GANDHI: {
        struct contract_completed cc_msg{};
        for (int i = 0; i < contracts_num; ++i) {
          MPI_Recv(&cc_msg, 1, MPI_CONTRACT_COMPETED_T, MPI_ANY_SOURCE, CONTRACT_COMPLETED,
                   MPI::COMM_WORLD,
                   &status);
          debug("I was informed that ELF %d has murdered all %d hamsters and so completed his contract (ID : %d)",
                status.MPI_SOURCE,
                contracts[cc_msg.contract_id].num_hamsters,
                cc_msg.contract_id)
          contracts_completed_flags[cc_msg.contract_id] = true;

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