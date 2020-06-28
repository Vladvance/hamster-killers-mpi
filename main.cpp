#include <mpi.h>
#include <iostream>
#include <cstddef>
#include <vector>
#include <random>
#include <algorithm>

#include "Common.h"

using namespace std;

void init() {
  //create contract type
  int contract_block_lengths[2] = {1, 1};
  MPI_Aint contract_offsets[2] = {offsetof(contract, contract_id),
                                  offsetof(contract, num_hamsters)};
  MPI_Datatype contract_types[2] = {MPI_INT, MPI_INT};
  MPI_Type_create_struct(2, contract_block_lengths, contract_offsets, contract_types, &MPI_CONTRACT_T);
  MPI_Type_commit(&MPI_CONTRACT_T);

  //create request_for_contract type
  int rfc_block_lengths[2] = {1, 1};
  MPI_Aint rfc_offsets[2] = {offsetof(request_for_contract, lamport_clock),
                             offsetof(request_for_contract, blood_hunger)};
  MPI_Datatype rfc_types[2] = {MPI_INT, MPI_INT};
  MPI_Type_create_struct(2, rfc_block_lengths, rfc_offsets, rfc_types, &MPI_REQUEST_FOR_CONTRACT_T);
  MPI_Type_commit(&MPI_REQUEST_FOR_CONTRACT_T);

  //create request_for_armor type
  int rfa_block_lengths[2] = {1, 1};
  MPI_Aint rfa_offsets[2] = {offsetof(request_for_armor, lamport_clock),
                             offsetof(request_for_armor, contract_id)};
  MPI_Datatype rfa_types[2] = {MPI_INT, MPI_INT};
  MPI_Type_create_struct(2, rfa_block_lengths, rfa_offsets, rfa_types, &MPI_REQUEST_FOR_ARMOR_T);
  MPI_Type_commit(&MPI_REQUEST_FOR_ARMOR_T);

  //create contract_completed type
  int cc_block_lengths[1] = {1};
  MPI_Aint cc_offsets[1] = {offsetof(contract_completed, contract_id)};
  MPI_Datatype cc_types[1] = {MPI_INT};
  MPI_Type_create_struct(1, cc_block_lengths, cc_offsets, cc_types, &MPI_CONTRACT_COMPETED_T);
  MPI_Type_commit(&MPI_CONTRACT_COMPETED_T);

  //create swap type
  int swap_block_lengths[2] = {1, 1};
  MPI_Aint swap_offsets[2] = {offsetof(swap_proc, rank_who_delegates),
                              offsetof(swap_proc, rank_to_whom_delegated)};
  MPI_Datatype swap_types[2] = {MPI_INT, MPI_INT};
  MPI_Type_create_struct(1, swap_block_lengths, swap_offsets, swap_types, &MPI_SWAP_T);
  MPI_Type_commit(&MPI_SWAP_T);

//    MPI_Type_commit(&MPI_ALLOCATE_ARMOR_T);
//    MPI_Type_commit(&MPI_DELEGATE_PRIORITY_T);
}

void finalize() {
  MPI_Type_free(&MPI_CONTRACT_T);
  MPI_Type_free(&MPI_REQUEST_FOR_CONTRACT_T);
  MPI_Type_free(&MPI_CONTRACT_COMPETED_T);
  MPI_Type_free(&MPI_REQUEST_FOR_ARMOR_T);
  MPI_Type_free(&MPI_SWAP_T);
  //test
//    MPI_Type_free(&MPI_ALLOCATE_ARMOR_T);
//    MPI_Type_free(&MPI_DELEGATE_PRIORITY_T);
  //test

  MPI::Finalize();
}

void showMessage(int rank, int lamport_clock, const char *message) {
  printf("[%d (%2d)]: %s\n", rank, lamport_clock, message);
}

void landlord_routine(int size, int rank) {

  std::random_device gen;
  std::uniform_int_distribution<int> rand_hamster_num(1, MAX_HUMSTERS_IN_CONTRACT);
  std::uniform_int_distribution<int> rand_contracts_num(1, size - 1);
  MPI_Status status;

  std::vector<struct contract> contracts;
  std::vector<bool> contracts_completed_flags;
  int contracts_num = -1;

  int state = HIRE;

  while (state != FINISH) {
    switch (state) {
      case HIRE: {

        //num of contracts equal num of elves, FOR TESTING PURPOSE
        contracts_num = size - 1;
//                    contracts_num = rand_contracts_num(gen);
        contracts.resize(contracts_num);
        contracts_completed_flags.resize(contracts_num);

        //generate random contracts
        for (int i = 0; i < contracts_num; ++i) {
          contracts[i].contract_id = i;
          contracts[i].num_hamsters = rand_hamster_num(gen);
        }

        //send contracts to elves
        for (int dst_rank = 1; dst_rank < size; ++dst_rank) {
          MPI_Send(contracts.data(), contracts_num, MPI_CONTRACT_T, dst_rank, CONTRACTS, MPI::COMM_WORLD);
        }

        state = READ_GANDHI;
        break;
      }
      case READ_GANDHI: {
        struct contract_completed cc_msg{};
        for (int i = 0; i < contracts_num; ++i) {
          MPI_Recv(&cc_msg, 1, MPI_CONTRACT_COMPETED_T, MPI_ANY_SOURCE, CONTRACT_COMPLETED,
                   MPI::COMM_WORLD,
                   &status);
          contracts_completed_flags[cc_msg.contract_id] = true;
        }
        state = FINISH;
        break;
      }
      default: {
        //should never reach here
        showMessage(rank, 0, "Entered superposition state. Exiting.");
        finalize();
        exit(EXIT_FAILURE);
      }
    }
  }
}

void elf_routine(int size, int rank) {

  std::vector<struct contract> contracts;
  std::vector<struct contract_queue_item> contract_queue;
  contract_queue.reserve(size - 1);
  std::vector<struct armory_allocation_item> armory_allocation_queue;
  armory_allocation_queue.reserve(size - 1);

  const int A = NUM_OF_SWORDS;
  const int T = NUM_OF_POISON_KITS;
  MPI_Status status;

  int contracts_num = -1;
  int poison_kits_needed = 0;
  int swords_needed = 0;
  int lamport_clock = 0;
  int blood_hunger = 0;
  int my_contract_id = -1;

  int state = PEACE_IS_A_LIE;

  while (state != FINISH) {
    switch (state) {
      case PEACE_IS_A_LIE: {
        //receive contracts from landlord
        MPI_Probe(LANDLORD_RANK, CONTRACTS, MPI::COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_CONTRACT_T, &contracts_num);

        if (contracts_num != MPI_UNDEFINED) {
          contracts.resize(contracts_num);
        } else {
          //should never reach here.
          showMessage(rank, lamport_clock, "Invalid contracts number. Exiting.");
          finalize();
          exit(EXIT_FAILURE);
        }

        MPI_Recv(contracts.data(), contracts_num, MPI_CONTRACT_T, LANDLORD_RANK, CONTRACTS,
                 MPI::COMM_WORLD,
                 &status);

        //DEBUG
        puts("CONTRACT LIST:");
        for (const auto &c : contracts) {
          printf("ID: %d : %d hamsters |", c.contract_id, c.num_hamsters);
        }

        struct request_for_contract rfc_msg{lamport_clock, blood_hunger};

        contract_queue.push_back({rank, rfc_msg});

        //send REQUEST_FOR_CONTRACT to the rest of elves
        for (int dst_rank = 1; dst_rank < size; ++dst_rank) {
          if (dst_rank == rank) continue;
          rfc_msg.lamport_clock++; //??
          MPI_Send(&rfc_msg, 1, MPI_REQUEST_FOR_CONTRACT_T, dst_rank, REQUEST_FOR_CONTRACT, MPI::COMM_WORLD);
        }

        state = GATHER_PARTY;
        break;
      }
      case GATHER_PARTY: {

        while (contract_queue.size() != (size - 1)) {
          struct contract_queue_item item{};
          MPI_Recv(&(item.rfc), 1, MPI_REQUEST_FOR_CONTRACT_T, MPI_ANY_SOURCE, REQUEST_FOR_CONTRACT,
                   MPI::COMM_WORLD, &status);
          item.rank = status.MPI_SOURCE;
          lamport_clock = std::max(lamport_clock, item.rfc.lamport_clock);
          contract_queue.push_back(item);
        }

        auto last_contract_it = std::next(contract_queue.begin(), contracts_num);
        std::partial_sort(contract_queue.begin(), last_contract_it, contract_queue.end());
        auto position_in_queue = std::find_if(contract_queue.begin(), contract_queue.end(),
                                              [=](const contract_queue_item &item) {
                                                return item.rank == rank;
                                              });


        //if we didn't get contract, increase blood_hunger and change state to PEACE_IS_A_LIE
        if (position_in_queue > last_contract_it) {
          blood_hunger++;
//          state = PEACE_IS_A_LIE;
          state = FINISH;
          break;
        }

        //if we got contract, send REQUEST_FOR_ARMOR to all processes with contracts
        my_contract_id = std::distance(contract_queue.begin(), position_in_queue);
        struct request_for_armor rfa_msg{lamport_clock, my_contract_id};

        //add corresponding item to armory_allocation_queue;
        armory_allocation_queue.push_back({rank, rfa_msg});

        for (auto it = contract_queue.begin(); it <= last_contract_it; ++it) {
          rfa_msg.lamport_clock++; //??
          MPI_Send(&rfa_msg, 1, MPI_REQUEST_FOR_ARMOR_T, it->rank, REQUEST_FOR_ARMOR, MPI::COMM_WORLD);
        }

        //init helper variables
        poison_kits_needed = std::accumulate(contracts.begin(), contracts.end(), 0,
                                             [](int sum, const struct contract &curr) {
                                               return sum + curr.num_hamsters;
                                             });
        swords_needed = contracts_num;

        state = TAKING_INVENTORY;
        break;
      }
      case TAKING_INVENTORY: {

        int rank_to_delegate = -1;

        while (true) {

          if (swords_needed < A && poison_kits_needed < T) {
            //send ALLOCATE_ARMOR to other elves
            for (int elf_rank = 1; elf_rank < size; ++elf_rank) {
              if (elf_rank == rank) continue;
              MPI_Send(nullptr, 0, MPI_ALLOCATE_ARMOR_T, elf_rank, DELEGATE_PRIORITY, MPI::COMM_WORLD);
            }
            state = RAMPAGE;
            break;
          }

          //if we've collected all ARMORY_ALLOCATION messages
          if (armory_allocation_queue.size() == contracts_num) {

            //if there is at least one free sword
            if (swords_needed > 0) { //??

              //check if there is contract which has less or equal num of targets then num of poison kits available
              for (const auto &armory_item : armory_allocation_queue) {
                if (contracts[armory_item.rfa.contract_id].num_hamsters
                    <= (T - poison_kits_needed)) { //nie jestem pewien co do warunku
                  //choose contract owner as elf to delegate priority
                  rank_to_delegate = contracts[armory_item.rfa.contract_id].contract_id;
                  break;
                }
              }

              if (rank_to_delegate != -1) {
                //send DELEGATE_PRIORITY message
                MPI_Send(nullptr, 0, MPI_DELEGATE_PRIORITY_T, rank_to_delegate, DELEGATE_PRIORITY,
                         MPI::COMM_WORLD);
                //check for all received messages
                bool is_done = false;
                while (!is_done) {
                  MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI::COMM_WORLD, &status);
                  switch (status.MPI_TAG) {
                    case DELEGATE_PRIORITY:
                      //dummy receive (nie wiem czy potrzebny)
                      MPI_Recv(nullptr, 0, MPI_DELEGATE_PRIORITY_T, status.MPI_SOURCE,
                               DELEGATE_PRIORITY,
                               MPI::COMM_WORLD, &status);
                      break;
                    case SWAP: {
                      struct swap_proc swap_msg{};
                      MPI_Recv(&swap_msg, 1, MPI_SWAP_T, status.MPI_SOURCE, SWAP, MPI::COMM_WORLD,
                               &status);
                      //if SWAP message contains contract_id we want to swap with, start swap procedure
                      if (swap_msg.rank_who_delegates == rank &&
                          swap_msg.rank_to_whom_delegated == rank_to_delegate) {
                        is_done = true;
                      }

                      auto delegating_elf_it = std::find_if(armory_allocation_queue.begin(),
                                                            armory_allocation_queue.end(),
                                                            [&swap_msg](const armory_allocation_item &aa_item) {
                                                              return aa_item.rank == swap_msg.rank_who_delegates;
                                                            });
                      //if delegating process is not yet in allocation_queue
                      if (delegating_elf_it == armory_allocation_queue.end()) {
                        //TODO: Implement postpone
                      } else {
                        auto delegated_elf_it = std::find_if(delegating_elf_it,
                                                             armory_allocation_queue.end(),
                                                             [&swap_msg](const armory_allocation_item &aa_item) {
                                                               return aa_item.rank == swap_msg.rank_to_whom_delegated;
                                                             });
                        if (rank == swap_msg.rank_who_delegates) {
                          int poison_kits_gain = std::accumulate(++delegating_elf_it,
                                                                 delegated_elf_it,
                                                                 0,
                                                                 [&contracts](int sum,
                                                                              const armory_allocation_item &aa_item) {
                                                                   return sum
                                                                       + contracts[aa_item.rfa.contract_id].num_hamsters;
                                                                 });
                          poison_kits_needed += poison_kits_gain;
                        } else {
                          auto my_pos = std::find_if(delegating_elf_it,
                                                     delegated_elf_it,
                                                     [=](const armory_allocation_item &aa_item) {
                                                       return aa_item.rank == rank;
                                                     });
                          if (my_pos
                              != delegated_elf_it) { // if my position in armory_allocation_queue is between swapping elves
                            int poison_kits_diff = contracts[delegating_elf_it->rfa.contract_id].num_hamsters -
                                contracts[delegated_elf_it->rfa.contract_id].num_hamsters;
                            poison_kits_needed += poison_kits_diff;
                          }
                        }
                      }
                      break;
                    }
                    case ALLOCATE_ARMOR: {
                      //dummy receive (nie wiem czy potrzebny)
                      MPI_Recv(nullptr, 0, MPI_ALLOCATE_ARMOR_T, status.MPI_SOURCE,
                               ALLOCATE_ARMOR,
                               MPI::COMM_WORLD, &status);
                      //if ALLOCATE_ARMOR message received from elf we wanted to swap with - break swap procedure
                      if (status.MPI_SOURCE == rank_to_delegate)
                        is_done = true;
                      break;
                    }
                    default:break;
                  }
                }
              }
            }
          }

          bool is_done = false;
          while (!is_done) {
            //check for all received messages
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI::COMM_WORLD, &status);
            switch (status.MPI_TAG) {
              case REQUEST_FOR_ARMOR: {
                struct request_for_armor rfa{}, my_rfa{my_contract_id, lamport_clock};
                MPI_Recv(&rfa, 1, MPI_REQUEST_FOR_ARMOR_T, status.MPI_SOURCE, REQUEST_FOR_ARMOR,
                         MPI::COMM_WORLD, &status);
                auto position = std::upper_bound(armory_allocation_queue.begin(),
                                                 armory_allocation_queue.end(),
                                                 rfa);
                armory_allocation_queue.insert(position, {status.MPI_SOURCE, rfa});
                auto my_position = std::lower_bound(armory_allocation_queue.begin(),
                                                    armory_allocation_queue.end(),
                                                    my_rfa);
                if (my_position < position) {
                  swords_needed--;
                  poison_kits_needed -= contracts[rfa.contract_id].num_hamsters;
                }

                break;
              }
              case CONTRACT_COMPLETED: {
                struct contract_completed cc_msg{};
                MPI_Recv(&cc_msg, 1, MPI_CONTRACT_COMPETED_T, status.MPI_SOURCE, CONTRACT_COMPLETED,
                         MPI::COMM_WORLD, &status);
                auto position_to_delete = std::find_if(armory_allocation_queue.begin(),
                                                       armory_allocation_queue.end(),
                                                       [&cc_msg](const request_for_armor &rfa_msg) {
                                                         return rfa_msg.contract_id == cc_msg.contract_id;
                                                       });
                armory_allocation_queue.erase(position_to_delete);
                swords_needed--;
                poison_kits_needed -= contracts[cc_msg.contract_id].num_hamsters;
                break;
              }
              case DELEGATE_PRIORITY: {
                const struct swap_proc swap_msg{rank, status.MPI_SOURCE};
                for (const armory_allocation_item &aa_item : armory_allocation_queue) {
                  MPI_Send(&swap_msg, 0, MPI_SWAP_T, aa_item.rank, SWAP,
                           MPI::COMM_WORLD);
                }
                is_done = true;
                state = RAMPAGE;
                break;
              }
              case SWAP: {
                struct swap_proc swap_msg{};
                MPI_Recv(&swap_msg, 1, MPI_SWAP_T, status.MPI_SOURCE, SWAP,
                         MPI::COMM_WORLD, &status);

                if (swap_msg.rank_who_delegates == rank &&
                    swap_msg.rank_to_whom_delegated == rank_to_delegate) {
                  is_done = true;
                }

                auto delegating_elf_it = std::find_if(armory_allocation_queue.begin(),
                                                      armory_allocation_queue.end(),
                                                      [&swap_msg](const armory_allocation_item &aa_item) {
                                                        return aa_item.rank == swap_msg.rank_who_delegates;
                                                      });
                //if delegating process is not yet in allocation_queue
                if (delegating_elf_it == armory_allocation_queue.end()) {
                  //TODO: Implement postpone
                } else {
                  auto delegated_elf_it = std::find_if(delegating_elf_it,
                                                       armory_allocation_queue.end(),
                                                       [&swap_msg](const armory_allocation_item &aa_item) {
                                                         return aa_item.rank == swap_msg.rank_to_whom_delegated;
                                                       });
                  if (rank == swap_msg.rank_who_delegates) {
                    int poison_kits_gain = std::accumulate(++delegating_elf_it,
                                                           delegated_elf_it,
                                                           0,
                                                           [&contracts](int sum,
                                                                        const armory_allocation_item &aa_item) {
                                                             return sum
                                                                 + contracts[aa_item.rfa.contract_id].num_hamsters;
                                                           });
                    poison_kits_needed += poison_kits_gain;
                  } else {
                    auto my_pos = std::find_if(delegating_elf_it,
                                               delegated_elf_it,
                                               [=](const armory_allocation_item &aa_item) {
                                                 return aa_item.rank == rank;
                                               });

                    // if my position in armory_allocation_queue is between swapping elves
                    if (my_pos != delegated_elf_it) {
                      int poison_kits_diff = contracts[delegating_elf_it->rfa.contract_id].num_hamsters -
                          contracts[delegated_elf_it->rfa.contract_id].num_hamsters;
                      poison_kits_needed += poison_kits_diff;
                    }
                  }
                  is_done = true;
                }
              } // case SWAP
              default:break;
            } // switch (MPI_TAG)
          } // while (!is_done)
        } // while (true)
        break;
      } // case TAKING_INVENTORY
      case RAMPAGE: {

        state = FINISH;
        break;
      }
      default:
        //should never reach here
        showMessage(rank, lamport_clock, "Entered superposition state. Exiting.");
        finalize();
        exit(EXIT_FAILURE);
    }
  }
}


int main(int argc, char **argv) {
  int size, rank;
  MPI::Init(argc, argv);
  init();

  size = MPI::COMM_WORLD.Get_size();
  rank = MPI::COMM_WORLD.Get_rank();

  if (rank == LANDLORD_RANK) { //if landlord
    landlord_routine(size, rank);
  } else { //if elf
    elf_routine(size, rank);
  }

  finalize();
}
