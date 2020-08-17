//
// Created by vladvance on 05.08.2020.
//

#include <algorithm>
#include <random>
#include <vector>
#include <mpl/mpl.hpp>
#include <queue>

#include "common.h"
#include "mpi_types.h"

static int rank;
static int size;
static int lamport_clock;

int get_contracts_from_landlord(const mpl::communicator &comm_world, std::vector<struct contract> &contracts) {
  debug("Looking forward for new contracts")
  auto status = comm_world.probe(LANDLORD_RANK, CONTRACTS);
  auto contracts_num = status.get_count<contract>();

  if (contracts_num != mpl::undefined) {
    contracts.resize(contracts_num);
  } else {
    // Should never reach here.
    debug("Invalid contracts number. Committing suicide.")
  }

  // Receive contracts from landlord
  comm_world.recv(contracts.begin(), contracts.end(), LANDLORD_RANK, CONTRACTS);
  debug("Received contract list.")
  return contracts_num;
}

void broadcast_rfc(const mpl::communicator &comm_world,
                   std::vector<struct contract_queue_item> &contract_queue,
                   int blood_hunger) {
  debug("Broadcasting REQUEST_FOR_CONTRACT to other gnomes")
  for (int dst_rank = 1; dst_rank < size; ++dst_rank) {
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

void receive_all_rfc(const mpl::communicator &comm_world, std::vector<struct contract_queue_item> &contract_queue) {
  for (auto cq_it = std::next(contract_queue.begin()); cq_it != contract_queue.end(); ++cq_it) {

    const auto &status = comm_world.recv(cq_it->rfc, mpl::any_source, REQUEST_FOR_CONTRACT);

    cq_it->rank = status.source();
    lamport_clock = std::max(lamport_clock, cq_it->rfc.lamport_clock) + 1;

    debug("Received REQUEST_FOR_CONTRACT [ LAMPORT_CLOCK: %d ] from GNOME %d",
          cq_it->rfc.lamport_clock,
          cq_it->rank)
  }
}

int get_contractid(std::vector<struct contract_queue_item> &contract_queue, int contracts_num) {
  const auto last_contract_it = std::next(contract_queue.begin(), contracts_num);
  std::partial_sort(contract_queue.begin(), last_contract_it, contract_queue.end());
  const auto position_in_queue_it = std::find_if(contract_queue.begin(), contract_queue.end(),
                                       [=](const contract_queue_item &item) {
                                                           return item.rank == rank;
                                                         });
  if(position_in_queue_it > last_contract_it)
    return CONTRACTID_UNDEFINED;
  else
    return std::distance(contract_queue.begin(), position_in_queue_it);
}

void broadcast_rfa(const mpl::communicator &comm_world,
                   std::vector<struct contract_queue_item> &contract_queue,
                   std::vector<struct armory_allocation_item> &armory_allocation_queue,
                   int my_contract_id,
                   int contracts_num) {
  const auto last_contract_it = std::next(contract_queue.begin(), contracts_num);
  debug("Broadcasting REQUEST_FOR_ARMOR to other gnomes")
  for (auto it = contract_queue.begin(); it < last_contract_it; ++it) {
    const struct request_for_armor rfa_msg{++lamport_clock, my_contract_id};

    if (it->rank == rank) {
      armory_allocation_queue.emplace_back(rank, rfa_msg);
      continue;
    }

//    debug("Sending REQUEST_FOR_ARMOR to GNOME %d", it->rank)
    comm_world.send(rfa_msg, it->rank, REQUEST_FOR_ARMOR);
  }
}

void broadcast_aa(const mpl::communicator &comm_world, const int gnomes_num) {
  debug("There are enough stuff available.")
  debug("Broadcasting ALLOCATE_ARMOR to other gnomes")
  for (int dst_rank = 1; dst_rank <= gnomes_num; ++dst_rank) {
    if (dst_rank == rank) continue;

//    debug("Sending ALLOCATE_ARMOR to GNOME %d", dst_rank)

    const struct allocate_armor aa_msg{};
    comm_world.send(aa_msg, dst_rank, ALLOCATE_ARMOR);
  }
}

void receive_rfa(const mpl::communicator &comm_world,
                 std::vector<struct armory_allocation_item> &armory_allocation_queue,
                 int source) {
  struct request_for_armor rfa{};
  comm_world.recv(rfa, source, REQUEST_FOR_ARMOR);
  debug("Received REQUEST_FOR_ARMOR from GNOME %d", source)

  armory_allocation_queue.emplace_back(source, rfa);
}

void gnome_routine(const mpl::communicator &comm_world) {
  rank = comm_world.rank();
  size = comm_world.size();
  const int gnomes_num = size - 1;

  std::vector<struct contract> contracts;
  std::vector<struct contract_queue_item> contract_queue(gnomes_num);
  std::vector<struct armory_allocation_item> armory_queue;
  armory_queue.reserve(gnomes_num);
  std::vector<bool> completed_contracts(gnomes_num);
  std::vector<bool> ranks_in_rampage(gnomes_num);
  std::priority_queue<struct armory_allocation_item> aa_queue;

  const int A = NUM_OF_SWORDS;
  const int T = NUM_OF_POISON_KITS;

  int contracts_num;
  int poison_kits_needed;
  int swords_needed;
  int blood_hunger = 0;
  int my_contract_id;

  debug("I'm alive!")

  // Set initial state
  int state = PEACE_IS_A_LIE;

  while (state != FINISH) {
    switch (state) {
      case PEACE_IS_A_LIE: {
        lamport_clock = 0;
        std::fill(completed_contracts.begin(), completed_contracts.end(), false);
        std::fill(ranks_in_rampage.begin(), ranks_in_rampage.end(), false);

        contracts_num = get_contracts_from_landlord(comm_world, contracts);

        // Send REQUEST_FOR_CONTRACT to the rest of gnomes
        broadcast_rfc(comm_world, contract_queue, blood_hunger);

        state = GATHER_PARTY;
        break;
      }
      case GATHER_PARTY: {

        // Get REQUEST_FOR_CONTRACT from other gnomes
        receive_all_rfc(comm_world, contract_queue);

        // Determine my contract id
        my_contract_id = get_contractid(contract_queue, contracts_num);

        debug("Contract queue:")
        for (const auto &c : contract_queue)
          debug("[ RANK: %d; LAMPORT_CLOCK: %d; BLOOD_HUNGER: %d]", c.rank, c.rfc.lamport_clock, c.rfc.blood_hunger)

        // If we didn't get contract, increase blood_hunger and change state to PEACE_IS_A_LIE
        if (my_contract_id == CONTRACTID_UNDEFINED) {
          debug("No work for me. Gonna rest a bit.")

          blood_hunger++;
//          state = PEACE_IS_A_LIE;
          state = FINISH;
          break;
        }

        debug("Determined my contract id: %d", my_contract_id)

        // If we got contract, send REQUEST_FOR_ARMOR to all processes with contracts
        broadcast_rfa(comm_world, contract_queue, armory_queue, my_contract_id, contracts_num);

        debug("Gonna take some stuff from armoury.")
        state = TAKING_INVENTORY;
        break;
      }
      case TAKING_INVENTORY: {

        if (armory_queue.size() == contracts_num &&
            swords_needed < A && poison_kits_needed < T) {
          // Send ALLOCATE_ARMOR to other gnomes
          broadcast_aa(comm_world, gnomes_num);
          debug("I'm ready TO KILL!!!")
          state = RAMPAGE;
          break;
        }

        const auto &status = comm_world.probe(mpl::any_source, mpl::tag::any());

        switch (static_cast<int>(status.tag())) {
          case REQUEST_FOR_ARMOR: {

            receive_rfa(comm_world, armory_queue, status.source());

            // If armory_queue is complete, calculate swords_needed and poison_kits_needed.
            if (armory_queue.size() == contracts_num) {

              std::sort(armory_queue.begin(), armory_queue.end());
              debug("armory_queue is complete.")
              for(const auto& item: armory_queue)
                debug("[ CLOCK: %d; CONTRACT_ID: %d, RANK: %d ]", item.rfa.lamport_clock, item.rfa.contract_id, item.rank)
              const auto my_pos = std::find_if(armory_queue.begin(),
                                               armory_queue.end(),
                                               [=](const auto &item) { return item.rank == rank; });

              swords_needed = std::distance(armory_queue.begin(), my_pos);
              poison_kits_needed = std::accumulate(armory_queue.begin(),
                                                   my_pos, 0, [&](int sum, const auto &item) {
                    return (completed_contracts[item.rfa.contract_id]) ? sum : sum + contracts[item.rfa.contract_id].num_hamsters;
                  });

              const int my_pos_idx = std::distance(armory_queue.begin(), my_pos);

              debug("My position in aa_queue = %d, swords_needed = %d, poison_kits_needed = %d", my_pos_idx, swords_needed, poison_kits_needed);
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

            if(armory_queue.size() < contracts_num) {
              completed_contracts[cc_msg.contract_id] = true;
              continue;
            }
            swords_needed--;
            poison_kits_needed -= contracts[cc_msg.contract_id].num_hamsters;
            break;
          }
        }
        break;
      }
      case RAMPAGE: {
        debug( "Wildly murdered %d hamsters and completed my contract (CONTRACT_ID: %d)."
               "Sending confirmation to landlord.",
            contracts[my_contract_id].num_hamsters, my_contract_id)

        debug("Broadcasting CONTRACT_COMPLETE")

        struct contract_completed cc_msg{my_contract_id};
        for (int dst_rank = 0; dst_rank < size; ++dst_rank) {
          if (dst_rank == rank)
            continue;
          comm_world.send(cc_msg, dst_rank, CONTRACT_COMPLETED);
        }
        state = FINISH;
        break;
      }
      default: {
        // Should never reach here
        debug("Entered superposition state. Committing suicide.")

        return;
      }
    }
  }
  debug("No work left for brave warrior. Committing suicide.")
}