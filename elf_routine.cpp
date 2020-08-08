//
// Created by vladvance on 05.08.2020.
//

#include <algorithm>
#include <random>
#include <vector>
#include <mpi.h>

#include "common.h"
#include "mpi_types.h"

void elf_routine(const int size, const int rank) {
  debug("I'm alive!")

  std::vector<struct contract> contracts;
  std::vector<struct contract_queue_item> contract_queue(size - 1);
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

  // Set initial state
  int state = PEACE_IS_A_LIE;

  while (state != FINISH) {
    switch (state) {
      case PEACE_IS_A_LIE: {
        // Receive contracts from landlord
        MPI_Probe(LANDLORD_RANK, CONTRACTS, MPI::COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_CONTRACT_T, &contracts_num);

        if (contracts_num != MPI_UNDEFINED) {
          contracts.resize(contracts_num);
        } else {
          // Should never reach here.
          debug("Invalid contracts number. Committing suicide.")
          return;
        }

        MPI_Recv(contracts.data(), contracts_num, MPI_CONTRACT_T, LANDLORD_RANK, CONTRACTS,
                 MPI::COMM_WORLD,
                 &status);

        debug("Received contract list.")

        struct request_for_contract rfc_msg{lamport_clock, blood_hunger};


        // Send REQUEST_FOR_CONTRACT to the rest of elves
        for (int dst_rank = 1; dst_rank < size; ++dst_rank) {
          rfc_msg.lamport_clock_ = ++lamport_clock; //??
          if (dst_rank == rank) {
            contract_queue[0] = {rank, {lamport_clock, blood_hunger}};
            debug("Added my contract data to contract_queue")
            continue;
          }
          debug("Sending REQUEST_FOR_CONTRACT to ELF %d", dst_rank)
          MPI_Send(&rfc_msg, 1, MPI_REQUEST_FOR_CONTRACT_T, dst_rank, REQUEST_FOR_CONTRACT, MPI::COMM_WORLD);
        }

        state = GATHER_PARTY;
        break;
      }
      case GATHER_PARTY: {

        for (size_t queue_idx = 1; queue_idx < size - 1; queue_idx++) {
          MPI_Recv(&(contract_queue[queue_idx].rfc),
                   1,
                   MPI_REQUEST_FOR_CONTRACT_T,
                   MPI_ANY_SOURCE,
                   REQUEST_FOR_CONTRACT,
                   MPI::COMM_WORLD,
                   &status);

          contract_queue[queue_idx].rank = status.MPI_SOURCE;
          lamport_clock = std::max(lamport_clock, contract_queue[queue_idx].rfc.lamport_clock_) + 1;

          debug("Received REQUEST_FOR_CONTRACT [ LAMPORT_CLOCK: %d ] from ELF %d",
                contract_queue[queue_idx].rfc.lamport_clock_,
                contract_queue[queue_idx].rank)
        }

        auto last_contract_it = std::next(contract_queue.begin(), contracts_num);
        std::partial_sort(contract_queue.begin(), last_contract_it, contract_queue.end());
        debug("Contract queue:")
        for(auto& c : contract_queue)
          debug("[ RANK: %d, LAMPORT_CLOCK: %d]", c.rank, c.rfc.lamport_clock_)
        auto position_in_queue = std::find_if(contract_queue.begin(), contract_queue.end(),
                                              [=](const contract_queue_item &item) {
                                                return item.rank == rank;
                                              });


        //if we didn't get contract, increase blood_hunger and change state to PEACE_IS_A_LIE
        if (position_in_queue > last_contract_it) {
          blood_hunger++;
//          state = PEACE_IS_A_LIE;
          debug("No work for me. Gonna rest a bit.")
          state = FINISH;
          break;
        }

        //if we got contract, send REQUEST_FOR_ARMOR to all processes with contracts
        my_contract_id = std::distance(contract_queue.begin(), position_in_queue);
        debug("Determined my contract id: %d", my_contract_id)
        struct request_for_armor rfa_msg{lamport_clock, my_contract_id};

        //add corresponding item to armory_allocation_queue;
        armory_allocation_queue.push_back({rank, rfa_msg});

        for (auto it = contract_queue.begin(); it < last_contract_it; ++it) {
          rfa_msg.lamport_clock++; //??
          debug("Sending REQUEST_FOR_ARMOR to ELF %d", it->rank)
          MPI_Send(&rfa_msg, 1, MPI_REQUEST_FOR_ARMOR_T, it->rank, REQUEST_FOR_ARMOR, MPI::COMM_WORLD);
        }

        //init helper variables
        poison_kits_needed = std::accumulate(contracts.begin(), contracts.end(), 0,
                                             [](int sum, const struct contract &curr) {
                                               return sum + curr.num_hamsters;
                                             });
        swords_needed = contracts_num;

        debug("Gonna take some stuff from armoury.")
        state = TAKING_INVENTORY;
        break;
      }
      case TAKING_INVENTORY: {

        int rank_to_delegate = -1;

        while (true) {
          debug("Swords needed: %d, Poison kits needed: %d", swords_needed, poison_kits_needed)

          if (swords_needed < A && poison_kits_needed < T) {
            // Send ALLOCATE_ARMOR to other elves
            debug("There are enough stuff available.")
            for (int dst_rank = 1; dst_rank < size; ++dst_rank) {
              if (dst_rank == rank) continue;
              debug("Sending ALLOCATE_ARMOR to ELF %d", dst_rank)
              MPI_Send(nullptr, 0, MPI_ALLOCATE_ARMOR_T, dst_rank, DELEGATE_PRIORITY, MPI::COMM_WORLD);
            }
            debug("I'm ready TO KILL!!!")
            state = RAMPAGE;
            break;
          }

          // If we've collected all ARMORY_ALLOCATION messages from elves who have contracts
          if (armory_allocation_queue.size() == contracts_num) {

            // If there is at least one free sword
            if (swords_needed > 0) { //??

              // Check if there is contract which has less or equal num of targets then num of poison kits available
              for (const auto &armory_item : armory_allocation_queue) {
                if (contracts[armory_item.rfa.contract_id].num_hamsters
                    <= (T - poison_kits_needed)) { //nie jestem pewien co do warunku
                  // Choose contract owner as elf to delegate priority
                  rank_to_delegate = contracts[armory_item.rfa.contract_id].contract_id;
                  debug("There are not enough resources to complete my contract, but ELF %d may begin", rank_to_delegate)
                  break;
                }
              }

              if (rank_to_delegate != -1) {
                // Send DELEGATE_PRIORITY message
                debug("Delegating priority to ELF %d", rank_to_delegate)
                debug("Sending DELEGATE_PRIORITY to ELF %d", rank_to_delegate)
                MPI_Send(nullptr, 0, MPI_DELEGATE_PRIORITY_T, rank_to_delegate, DELEGATE_PRIORITY,
                         MPI::COMM_WORLD);
                // Check for all received messages
                bool is_done = false;
                while (!is_done) {
                  MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI::COMM_WORLD, &status);
                  switch (status.MPI_TAG) {
                    case DELEGATE_PRIORITY:
                      //dummy receive (nie wiem czy potrzebny)
                      MPI_Recv(nullptr, 0, MPI_DELEGATE_PRIORITY_T, status.MPI_SOURCE,
                               DELEGATE_PRIORITY,
                               MPI::COMM_WORLD, &status);
                      debug("Received DELEGATE_PRIORITY from ELF %d. Dropping.", status.MPI_SOURCE)
                      break;
                    case SWAP: {
                      struct swap_proc swap_msg{};
                      MPI_Recv(&swap_msg, 1, MPI_SWAP_T, status.MPI_SOURCE, SWAP, MPI::COMM_WORLD,
                               &status);
                      debug("Received SWAP [%d delegates to %d] from ELF %d.",swap_msg.rank_who_delegates, swap_msg.rank_to_whom_delegated, status.MPI_SOURCE)
                      // If SWAP message contains contract_id we want to swap with, start swap procedure
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
                struct armory_allocation_item other_rfa{};
                struct armory_allocation_item my_rfa{rank, {my_contract_id, lamport_clock}};
                MPI_Recv(&(other_rfa.rfa), 1, MPI_REQUEST_FOR_ARMOR_T, status.MPI_SOURCE, REQUEST_FOR_ARMOR,
                         MPI::COMM_WORLD, &status);
                auto position = std::upper_bound(armory_allocation_queue.begin(),
                                                 armory_allocation_queue.end(),
                                                 other_rfa);
                other_rfa.rank = status.MPI_SOURCE;
                armory_allocation_queue.insert(position, other_rfa);
                auto my_position = std::lower_bound(armory_allocation_queue.begin(),
                                                    armory_allocation_queue.end(),
                                                    my_rfa);
                if (my_position < position) {
                  swords_needed--;
                  poison_kits_needed -= contracts[other_rfa.rfa.contract_id].num_hamsters;
                }

                break;
              }
              case CONTRACT_COMPLETED: {
                struct contract_completed cc_msg{};
                MPI_Recv(&cc_msg, 1, MPI_CONTRACT_COMPETED_T, status.MPI_SOURCE, CONTRACT_COMPLETED,
                         MPI::COMM_WORLD, &status);
                auto position_to_delete = std::find_if(armory_allocation_queue.begin(),
                                                       armory_allocation_queue.end(),
                                                       [&cc_msg](const armory_allocation_item &rfa_msg) {
                                                         return rfa_msg.rfa.contract_id == cc_msg.contract_id;
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
        debug("Entered superposition state. Committing suicide.");
        return;
    }
  }
  debug("No work left for brave warrior. Committing suicide.");
}