//
// Created by vladvance on 05.08.2020.
//

#include <algorithm>
#include <random>
#include <vector>
#include <mpl/mpl.hpp>
#include <zconf.h>

#include "common.h"
#include "mpi_types.h"
#include "gnome.h"

void gnome::run() {

  debug("I'm alive!")

  // Set initial state
  int state = PEACE_IS_A_LIE;
  bool initiating_swap = false;
  int rank_to_delegate = -1;

  while (state != FINISH) {
    switch (state) {
      case PEACE_IS_A_LIE: {
        lamport_clock = 0;
        std::fill(is_completed.begin(), is_completed.end(), false);
        std::fill(ranks_in_rampage.begin(), ranks_in_rampage.end(), false);
        swap_queue.clear();

        contracts_num = get_contracts_from_landlord();

        // Send REQUEST_FOR_CONTRACT to the rest of gnomes
        broadcast_rfc();

        state = GATHER_PARTY;
        break;
      }
      case GATHER_PARTY: {

        // Get REQUEST_FOR_CONTRACT from other gnomes
        receive_all_rfc();

        // Determine my contract id
        my_contract_id = get_contractid();

        debug("Contract queue:")
        for (const auto &c : contract_queue)
          debug("[ RANK: %d; LAMPORT_CLOCK: %d; BLOOD_HUNGER: %d]", c.rank, c.rfc.lamport_clock, c.rfc.blood_hunger)

        // If we didn't get a contract, increase blood_hunger and change state to PEACE_IS_A_LIE
        if (my_contract_id == contractid_undefined) {
          debug("No work for me. Gonna rest a bit.")
          blood_hunger++;
//          state = PEACE_IS_A_LIE;
          state = FINISH;
          break;
        }

        debug("Determined my contract id: %d", my_contract_id)

        // If we got a contract, send REQUEST_FOR_ARMOR to all gnomes with contracts
        broadcast_rfa();

        debug("Gonna take some stuff from armoury.")
        state = TAKING_INVENTORY;
        break;
      }
      case TAKING_INVENTORY: {

        // If armory_queue is complete (gnome received REQUEST_FROM_ARMOR from all gnomes)
        if (armory_queue.size() == contracts_num) {
          if (swords_needed < S && poison_kits_needed < P) {
            // Send ALLOCATE_ARMOR to other gnomes
            broadcast_aa();
            debug("I'm ready TO KILL!!!")
            state = RAMPAGE;
            break;
          }
          if (initiating_swap) {
            struct delegate_priority dp_msg;
            comm_world.send(dp_msg, rank_to_delegate, DELEGATE_PRIORITY);

            const auto &status = comm_world.probe(mpl::any_source, mpl::tag::any());

            switch (static_cast<int>(status.tag())) {
              case SWAP:
              case ALLOCATE_ARMOR: {
                // If gnome probed SWAP or ALLOCATE_ARMOR from gnome he is delegating to, move from initializing to swapping phase
                if (status.tag() == ALLOCATE_ARMOR) {
                  // Dummy receive
                  struct allocate_armor dummy;
                  comm_world.recv(dummy, status.source(), ALLOCATE_ARMOR);
                }

                /*FIXME: Teoretycznie w nastepnej iteracji skrzat powinien za'probe'bować ten sam SWAP
                 * w innym switchu i tam go obsłużyć
                 */
                if (status.source() == rank_to_delegate)
                  initiating_swap = false;
              }
                break;
              case DELEGATE_PRIORITY: {
                // Dummy receive
                struct delegate_priority dummy;
                comm_world.recv(dummy, status.source(), DELEGATE_PRIORITY);
              }
                break;
              default: {
                // Should never reach here
//                debug("Entered superposition state. Committing suicide.")
//                return;
              }
            }

          }
        }

        const auto &status = comm_world.probe(mpl::any_source, mpl::tag::any());

        switch (static_cast<int>(status.tag())) {
          case REQUEST_FOR_ARMOR: {
            receive_rfa(status.source());
            // If armory_queue is complete, calculate swords_needed and poison_kits_needed.
            if (armory_queue.size() == contracts_num) {
              std::sort(armory_queue.begin(), armory_queue.end());
              for (auto swap_msg: swap_queue) {
                auto swap1 = aa_queue_find_position(swap_msg.rank_who_delegates);
                auto swap2 = aa_queue_find_position(swap_msg.rank_to_whom_delegated);
                std::swap(swap1, swap2);
              }
              // Print armory_queue
              debug("Armory queue:")
              for (const auto &item: armory_queue)
                debug("[ CLOCK: %2d; RANK: %2d; COMPLETED: %s; CONTRACT_ID: %2d; NUM_HAMSTERS: %2d ]",
                      item.rfa.lamport_clock,
                      item.rank,
                      (is_completed[item.rfa.contract_id]) ? "true" : "false",
                      item.rfa.contract_id,
                      contracts[item.rfa.contract_id].num_hamsters)

              // Get my position in armory_queue
              my_aaq_pos = aa_queue_find_position(rank);

              // Calculate variables
              swords_needed = std::distance(armory_queue.begin(), my_aaq_pos);
              poison_kits_needed = std::accumulate(armory_queue.begin(),
                                                   my_aaq_pos, 0, [&](int sum, const auto &item) {
                    return (is_completed[item.rfa.contract_id]) ? sum : sum
                        + contracts[item.rfa.contract_id].num_hamsters;
                  });


              const int my_pos_idx = std::distance(armory_queue.begin(), my_aaq_pos);
              debug("My position in armory_queue = %d, swords_needed = %d, poison_kits_needed = %d",
                    my_pos_idx,
                    swords_needed,
                    poison_kits_needed)

              // Check if can swap with somebody
              if (poison_kits_needed > P &&
                  my_aaq_pos != std::prev(armory_queue.end())) {
                debug("Checking if I can swap with somebody")

                rank_to_delegate = get_rank_to_delegate();
                if(rank_to_delegate != -1) {
                  initiating_swap = true;
                  struct delegate_priority dp_msg;
                  comm_world.send(dp_msg, rank_to_delegate, DELEGATE_PRIORITY);
                  break;
                }
              }
            }
            break;
          }
          case ALLOCATE_ARMOR: {
            debug("Received ALLOCATE ARMOR from GNOME %d", status.source())
            struct allocate_armor aa_msg{};
            // Dummy receive
            comm_world.recv(aa_msg, status.source(), ALLOCATE_ARMOR);
            ranks_in_rampage[status.source()] = true;
            break;
          }
          case CONTRACT_COMPLETED: {
            struct contract_completed cc_msg{};
            comm_world.recv(cc_msg, status.source(), CONTRACT_COMPLETED);
            debug("Received CONTRACT_COMPLETED from GNOME %d.",
                  status.source())
            if (armory_queue.size() < contracts_num) {
              is_completed[cc_msg.contract_id] = true;
              continue;
            }
            swords_needed--;
            poison_kits_needed -= contracts[cc_msg.contract_id].num_hamsters;
          }
            break;
          case DELEGATE_PRIORITY: {
            // Send SWAP to all gnomes between me and delegating gnome
            struct delegate_priority dummy;
            comm_world.recv(dummy, status.source(), DELEGATE_PRIORITY);
            const auto delegating_pos = aa_queue_find_position(status.source());
            for (auto it = delegating_pos; it != my_aaq_pos; ++it) {
              const struct swap_proc swap_msg{status.source(), rank};
              comm_world.send(swap_msg, it->rank, SWAP);
            }
            state = RAMPAGE;
          }
            break;
          case SWAP: {
            struct swap_proc swap_msg;
            comm_world.recv(swap_msg, status.source(), SWAP);
            handle_swap(swap_msg);
          }
            break;
        }
      }
        break;
      case RAMPAGE: {
        // Sleep time proportional to number of hamsters to kill. *fairness noises*
        usleep(contracts[my_contract_id].num_hamsters * 1e5);
        debug("Wildly murdered %d hamsters and completed my contract (CONTRACT_ID: %d).",
              contracts[my_contract_id].num_hamsters, my_contract_id)
        debug("Broadcasting CONTRACT_COMPLETE")
        broadcast_cc();

        blood_hunger = 0;
        state = FINISH;
        break;
      }
      default: {
        // Should never reach here
        debug("Entered superposition state. Committing suicide.")
        return;
      }
    } // switch(state)
  } // while(state != FINISH)
  debug("No work left for brave warrior. Committing suicide.")
}

void gnome::broadcast_cc() const {
  struct contract_completed cc_msg{my_contract_id};
  for (int dst_rank = 0; dst_rank < size; ++dst_rank) {
    if (dst_rank == rank)
      continue;
    comm_world.send(cc_msg, dst_rank, CONTRACT_COMPLETED);
  }
}

std::vector<struct armory_allocation_item>::iterator gnome::aa_queue_find_position(const int rank_) {
  return std::find_if(armory_queue.begin(),
                      armory_queue.end(),
                      [=](const auto &item) { return item.rank == rank_; });
}

int gnome::get_contracts_from_landlord() {
  debug("Looking forward for new contracts")
  auto status = comm_world.probe(landlord_rank, CONTRACTS);
  contracts_num = status.get_count<contract>();

  if (contracts_num != mpl::undefined) {
    contracts.resize(contracts_num);
  } else {
    // Should never reach here.
    debug("Invalid contracts number. Committing suicide.")
  }

  // Receive contracts from landlord
  comm_world.recv(contracts.begin(), contracts.end(), landlord_rank, CONTRACTS);
  debug("Received contract list.")
  return contracts_num;
}

void gnome::broadcast_rfc() {
  debug("Broadcasting REQUEST_FOR_CONTRACT to other gnomes")
  for (int dst_rank = gnome_first; dst_rank < size; ++dst_rank) {
    const struct request_for_contract rfc_msg{++lamport_clock, blood_hunger};
    if (dst_rank == rank) {
      contract_queue.front() = {rank, rfc_msg};
//      debug("Added my contract data to contract_queue")
      continue;
    }
//    debug("Sending REQUEST_FOR_CONTRACT to GNOME %d", dst_rank)
    comm_world.send(rfc_msg, dst_rank, REQUEST_FOR_CONTRACT);
  }
}

void gnome::receive_all_rfc() {
  for (auto cq_it = std::next(contract_queue.begin()); cq_it != contract_queue.end(); ++cq_it) {

    const auto &status = comm_world.recv(cq_it->rfc, mpl::any_source, REQUEST_FOR_CONTRACT);

    cq_it->rank = status.source();
    lamport_clock = std::max(lamport_clock, cq_it->rfc.lamport_clock) + 1;

    debug("Received REQUEST_FOR_CONTRACT [ LAMPORT_CLOCK: %d ] from GNOME %d",
          cq_it->rfc.lamport_clock,
          cq_it->rank)
  }
}

int gnome::get_contractid() {
  const auto last_contract_it = std::next(contract_queue.begin(), contracts_num);
  std::partial_sort(contract_queue.begin(), last_contract_it, contract_queue.end());
  const auto position_in_queue_it = std::find_if(contract_queue.begin(), contract_queue.end(),
                                                 [=](const contract_queue_item &item) {
                                                   return item.rank == rank;
                                                 });
  if (position_in_queue_it > last_contract_it)
    return contractid_undefined;
  else
    return std::distance(contract_queue.begin(), position_in_queue_it);
}

void gnome::broadcast_rfa() {
  const auto last_contract_it = std::next(contract_queue.begin(), contracts_num);
  debug("Broadcasting REQUEST_FOR_ARMOR to other gnomes")
  for (auto it = contract_queue.begin(); it < last_contract_it; ++it) {
    const struct request_for_armor rfa_msg{++lamport_clock, my_contract_id};

    if (it->rank == rank) {
      armory_queue.emplace_back(rank, rfa_msg);
      continue;
    }

//    debug("Sending REQUEST_FOR_ARMOR to GNOME %d", it->rank)
    comm_world.send(rfa_msg, it->rank, REQUEST_FOR_ARMOR);
  }
}

void gnome::broadcast_aa() {
  debug("There are enough stuff available.")
  debug("Broadcasting ALLOCATE_ARMOR to other gnomes")
  for (int dst_rank = 1; dst_rank <= gnomes_num; ++dst_rank) {
    if (dst_rank == rank) continue;
//    debug("Sending ALLOCATE_ARMOR to GNOME %d", dst_rank)
    const struct allocate_armor aa_msg{};
    comm_world.send(aa_msg, dst_rank, ALLOCATE_ARMOR);
  }
}

void gnome::receive_rfa(int source) {
  struct request_for_armor rfa{};
  comm_world.recv(rfa, source, REQUEST_FOR_ARMOR);
  debug("Received REQUEST_FOR_ARMOR from GNOME %d", source)
  armory_queue.emplace_back(source, rfa);
}

int gnome::get_rank_to_delegate() {
  // If exceeding even without this gnome, nobody can fit
  if((poison_kits_needed - contracts[my_contract_id].num_hamsters) >= P)
    return -1;
  int max_hamsters_fit = P - (poison_kits_needed - contracts[my_contract_id].num_hamsters);
  auto pos = std::find_if(std::next(my_aaq_pos), armory_queue.end(),
               [=](const auto& item) {return contracts[item.rfa.contract_id].num_hamsters <= max_hamsters_fit;});
  if(pos == armory_queue.end())
    return -1;
  else {
    debug("Initiating swap with %d, he has %d hamsters to kill, and I have %d",
          pos->rank,
          contracts[pos->rfa.contract_id].num_hamsters,
          contracts[my_contract_id].num_hamsters)
    return std::distance(armory_queue.begin(), pos);
  }
}

void gnome::handle_swap(const struct swap_proc swap_msg) {
  if (armory_queue.size() != contracts_num) {
    swap_queue.push_back(swap_msg);
    return;
  }
  std::vector<struct armory_allocation_item>::iterator swap1_pos;
  if (swap_msg.rank_who_delegates == rank)
    swap1_pos = my_aaq_pos;
  else
    swap1_pos = std::find_if(armory_queue.begin(),
                             armory_queue.end(),
                             [=](const auto &item) { return item.rank == swap_msg.rank_who_delegates; });
  auto swap2_pos = std::find_if(swap1_pos,
                                armory_queue.end(),
                                [=](const auto &item) { return item.rank == swap_msg.rank_to_whom_delegated; });
  if (swap1_pos == my_aaq_pos) {
    for (auto it = std::next(my_aaq_pos); it <= swap2_pos; ++it) {
      poison_kits_needed += contracts[it->rfa.contract_id].num_hamsters;
    }
  }
  //TODO: warunek będze zawsze spełniony, jeśli SWAP wysyłany tylko do procesów pomiędzy nami
  if (swap1_pos < my_aaq_pos && my_aaq_pos < swap2_pos) {
    poison_kits_needed -= contracts[swap1_pos->rfa.contract_id].num_hamsters
        - contracts[swap2_pos->rfa.contract_id].num_hamsters;
  }
  //TODO: TBH nie ma sensu bawić siœ z samą kolejką skoro wszystko sależy od poison_kts_needed
  std::swap(swap1_pos, swap2_pos);
}
