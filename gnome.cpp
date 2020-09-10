//
// Created by vladvance on 05.08.2020.
//

#include "gnome.h"

#include <unistd.h>

#include "common.h"
#include "landlord.h"
#include "mpi_types.h"

Gnome::Gnome(const mpl::communicator &communicator)
    : ProcessBase(communicator, "GNOME"),
      numberOfGnomes(communicator.size() - 1),
      bloodHunger(0) {}

void Gnome::run(int maxRounds) {
  log("I'm alive!");

  state = PEACE_IS_A_LIE;
  int round = 0;

  while (round != maxRounds) {
    switch (state) {
      case PEACE_IS_A_LIE: {
        doPeaceIsALie();
        break;
      }
      case GATHER_PARTY: {
        doGatherParty();
        break;
      }
      case TAKING_INVENTORY: {
        doTakingInventory();
        break;
      }
      case DELEGATING_PRIORITY: {
        doDelegatingPriority();
        break;
      }
      case RAMPAGE: {
        doRampage();
        break;
      }
      case FINISH: {
        round++;
        state = PEACE_IS_A_LIE;
        break;
      }
      default: {
        // should never reach here
        log("Entered superposition state. Committing suicide.");
        return;
      }
    }
  }
  log("No work left for brave warrior. Committing suicide.");
}

void Gnome::doPeaceIsALie() {
  log("Looking forward for new contracts");
  receiveVector(contracts, Landlord::landlordRank, CONTRACTS);
  log("Received contract list.");

  log("Broadcasting REQUEST_FOR_CONTRACT to other gnomes");
  RequestForContract request(bloodHunger);
  setBroadcastScope(getAllGnomeRanks());
  broadcast(request, REQUEST_FOR_CONTRACT);

  contractQueue.clear();
  contractQueue.push_back(ContractQueueItem{rank, request});

  state = GATHER_PARTY;
}

void Gnome::doGatherParty() {
  // Get REQUEST_FOR_CONTRACT from other gnomes
  while (contractQueue.size() < numberOfGnomes) {
    RequestForContract request{};
    const auto &status = receiveAny(request, REQUEST_FOR_CONTRACT);
    contractQueue.push_back(ContractQueueItem{status.source(), request});
    log("Received REQUEST_FOR_CONTRACT [ timestamp: %d ] from GNOME %d",
        request.timestamp, status.source());
  }

  // If we didn't get a contract, increase blood hunger and finish round
  if (!getContract()) {
    log("No work for me. Gonna rest a bit.");
    bloodHunger++;
    state = FINISH;
    return;
  }

  // If we got a contract, send REQUEST_FOR_ARMOR to all gnomes with contracts
  log("Determined my contract id: %d", currentContractId);
  log("Broadcasting REQUEST_FOR_ARMOR to other gnomes");
  RequestForArmor request(currentContractId);
  setBroadcastScope(getEmployedGnomeRanks());
  broadcast(request, REQUEST_FOR_ARMOR);

  armoryQueue.clear();
  armoryQueue.reserve(contracts.size());
  armoryQueue.push_back(ArmoryAllocationItem{rank, request});
  positionInArmoryQueue = armoryQueue.begin();

  swordsNeeded = numberOfGnomes;
  poisonNeeded = 0;
  for (auto contract : contracts) {
    poisonNeeded += contract.numberOfHamsters;
  }

  log("Gonna take some stuff from armoury.");
  state = TAKING_INVENTORY;
}

void Gnome::doTakingInventory() {
  if (swordsNeeded <= swordsTotal && poisonNeeded <= poisonTotal) {
    log("There is enough stuff available.");
    log("Broadcasting ALLOCATE_ARMOR to other gnomes");
    AllocateArmor message{};
    broadcast(message, ALLOCATE_ARMOR);
    log("I'm ready TO KILL!!!");
    state = RAMPAGE;
    return;
  }

  if (armoryQueue.size() == contracts.size()) {
    swapRank = findSwapCandidate();
    if (swapRank != rank) {
      DelegatePriority message;
      send(message, swapRank, DELEGATE_PRIORITY);
      state = DELEGATING_PRIORITY;
      return;
    }
  }

  std::unordered_map<
      int, std::function<void(const MessageBase *, const mpl::status &)>>
      messageHandlers{
          {REQUEST_FOR_ARMOR,
           [this](const MessageBase *message, const mpl::status &status) {
             handleRequestForArmor(message, status);
           }},
          {CONTRACT_COMPLETED,
           [this](const MessageBase *message, const mpl::status &status) {
             handleContractCompleted(message, status);
           }},
          {SWAP,
           [this](const MessageBase *message, const mpl::status &status) {
             handleSwap(message, status);
           }},
          {DELEGATE_PRIORITY,
           [this](const MessageBase *message, const mpl::status &status) {
             handleDelegatePriority(message, status);
           }},
          {ALLOCATE_ARMOR,
           [](const MessageBase *message, const mpl::status &status) {
             // receive and skip
           }}};

  receiveMultiTag(mpl::any_source, messageHandlers);
}

void Gnome::doDelegatingPriority() {
  std::unordered_map<
      int, std::function<void(const MessageBase *, const mpl::status &)>>
      messageHandlers{
          {SWAP,
           [this](const MessageBase *message, const mpl::status &status) {
             handleSwapDelegating(message, status);
           }},
          {ALLOCATE_ARMOR,
           [this](const MessageBase *message, const mpl::status &status) {
             handleAllocateArmorDelegating(message, status);
           }},
          {DELEGATE_PRIORITY,
           [](const MessageBase *message, const mpl::status &status) {
             // receive and skip
           }}};

  receiveMultiTag(mpl::any_source, messageHandlers);
}

void Gnome::doRampage() {
  // Sleep time proportional to number of hamsters to kill. *fairness noises*
  usleep(contracts[currentContractId].numberOfHamsters * 1e5);
  log("Wildly murdered %d hamsters and completed my contract "
      "(CONTRACT_ID: %d).",
      contracts[currentContractId].numberOfHamsters, currentContractId);
  log("Broadcasting CONTRACT_COMPLETE");
  ContractCompleted message(currentContractId);
  broadcast(message, CONTRACT_COMPLETED);
  send(message, Landlord::landlordRank, CONTRACT_COMPLETED);

  bloodHunger = 0;
  state = FINISH;
}

std::vector<int> Gnome::getAllGnomeRanks() {
  auto ranks = std::vector<int>(numberOfGnomes + 1);
  std::iota(ranks.begin(), ranks.end(), 0);
  ranks.erase(std::find(ranks.begin(), ranks.end(), Landlord::landlordRank));
  return ranks;
}

std::vector<int> Gnome::getEmployedGnomeRanks() {
  auto ranks = std::vector<int>(contracts.size());
  for (int i = 0; i < contracts.size(); i++) {
    ranks[i] = contractQueue[i].rank;
  }
  return ranks;
}

bool Gnome::getContract() {
  auto firstUnemployed = contractQueue.begin() + contracts.size();
  std::partial_sort(contractQueue.begin(), firstUnemployed,
                    contractQueue.end());
  auto myPosition = std::find_if(
      contractQueue.begin(), contractQueue.end(),
      [=](const ContractQueueItem &item) { return item.rank == rank; });

  log("Contract queue:");
  for (const auto &contract : contractQueue) {
    log("[ RANK: %d; LAMPORT_CLOCK: %d; BLOOD_HUNGER: %d ]", contract.rank,
        contract.request.timestamp, contract.request.bloodHunger);
  }

  if (myPosition >= firstUnemployed) {
    return false;
  }

  currentContractId =
      contracts[std::distance(contractQueue.begin(), myPosition)].contractId;
  return true;
}

int Gnome::findSwapCandidate() {
  int maxPoisonAvailable =
      poisonTotal -
      (poisonNeeded - contracts[currentContractId].numberOfHamsters);
  // If exceeding even without this gnome, nobody can fit
  if (maxPoisonAvailable <= 0 || swordsNeeded > swordsTotal) {
    return rank;
  }
  auto swapWith = std::find_if(
      std::next(positionInArmoryQueue), armoryQueue.end(),
      [this, maxPoisonAvailable](const auto &item) {
        return contracts[item.request.contractId].numberOfHamsters <=
               maxPoisonAvailable;
      });
  if (swapWith == armoryQueue.end()) {
    return rank;
  }
  log("Initiating swap with %d, he has %d hamsters to kill, and I have %d",
      swapWith->rank, contracts[swapWith->request.contractId].numberOfHamsters,
      contracts[currentContractId].numberOfHamsters);
  return swapWith->rank;
}

void Gnome::applySwap(const Swap &swap) {
  auto swap1 = std::find_if(
      armoryQueue.begin(), armoryQueue.end(),
      [swap](const auto &item) { return item.rank == swap.delegatingRank; });
  auto swap2 = std::find_if(
      armoryQueue.begin(), armoryQueue.end(),
      [swap](const auto &item) { return item.rank == swap.delegatedRank; });
  if (swap1 < positionInArmoryQueue && positionInArmoryQueue < swap2) {
    auto difference = contracts[swap1->request.contractId].numberOfHamsters -
                      contracts[swap2->request.contractId].numberOfHamsters;
    poisonNeeded -= difference;
  } else if (swap.delegatingRank == rank) {
    for (auto it = std::next(positionInArmoryQueue); it <= swap2; ++it) {
      poisonNeeded += contracts[it->request.contractId].numberOfHamsters;
    }
    positionInArmoryQueue = swap2;
  }

  std::swap(*swap1, *swap2);
}

void Gnome::handleRequestForArmor(const MessageBase *message,
                                  const mpl::status &status) {
  log("Received REQUEST_FOR_ARMOR from GNOME %d", status.source());
  auto &request = *static_cast<const RequestForArmor *>(message);
  ArmoryAllocationItem queueItem(status.source(), request);
  armoryQueue.push_back(queueItem);

  if ((*positionInArmoryQueue) < queueItem) {
    auto contractId = queueItem.request.contractId;
    swordsNeeded--;
    poisonNeeded -= contracts[contractId].numberOfHamsters;
    log("Updated my resource requirements: swords_needed = %d, "
        "poison_kits_needed = %d",
        swordsNeeded, poisonNeeded);
  }

  // If armory_queue is complete, sort it and apply deferred swaps
  if (armoryQueue.size() == contracts.size()) {
    std::sort(armoryQueue.begin(), armoryQueue.end());
    for (auto swap : swapQueue) {
      applySwap(swap);
    }
    swapQueue.clear();
    // Print armory_queue
    log("Armory queue:");
    for (const auto &item : armoryQueue) {
      log("[ CLOCK: %2d; RANK: %2d; CONTRACT_ID: "
          "%2d; NUM_HAMSTERS: %2d ]",
          item.request.timestamp, item.rank, item.request.contractId,
          contracts[item.request.contractId].numberOfHamsters);
    }

    positionInArmoryQueue =
        std::find_if(armoryQueue.begin(), armoryQueue.end(),
                     [=](const auto &item) { return item.rank == rank; });

    log("My position in armory_queue = %d, swords_needed = %d, "
        "poison_kits_needed = %d",
        std::distance(armoryQueue.begin(), positionInArmoryQueue), swordsNeeded,
        poisonNeeded);
  }
}

void Gnome::handleContractCompleted(const MessageBase *message,
                                    const mpl::status &status) {
  log("Received CONTRACT_COMPLETED from GNOME %d.", status.source());
  auto &report = *static_cast<const ContractCompleted *>(message);
  auto contractId = report.contractId;
  swordsNeeded--;
  poisonNeeded -= contracts[contractId].numberOfHamsters;
}

void Gnome::handleSwap(const MessageBase *message, const mpl::status &status) {
  log("Received SWAP from GNOME %d.", status.source());
  auto &swap = *static_cast<const Swap *>(message);
  if (armoryQueue.size() < contracts.size()) {
    swapQueue.push_back(swap);
    return;
  }
  applySwap(swap);
}

void Gnome::handleDelegatePriority(const MessageBase *message,
                                   const mpl::status &status) {
  log("Received DELEGATE_PRIORITY from GNOME %d.", status.source());
  static_cast<const DelegatePriority *>(message);
  auto swap = Swap(status.source(), rank);
  broadcast(swap, SWAP);
  state = RAMPAGE;
}

void Gnome::handleSwapDelegating(const MessageBase *message,
                                 const mpl::status &status) {
  log("Received SWAP from GNOME %d.", status.source());
  auto &swap = *static_cast<const Swap *>(message);
  applySwap(swap);
  if ((swap.delegatedRank == swapRank) || (swap.delegatingRank == swapRank)) {
    state = TAKING_INVENTORY;
  }
}

void Gnome::handleAllocateArmorDelegating(const MessageBase *message,
                                          const mpl::status &status) {
  log("Received ALLOCATE_ARMOR from GNOME %d.", status.source());
  static_cast<const AllocateArmor *>(message);
  if (status.source() == swapRank) {
    state = TAKING_INVENTORY;
  }
}

#include <algorithm>
#include <mpl/mpl.hpp>
#include <random>
#include <vector>

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

        debug("Contract queue:") for (const auto &c : contract_queue)
            debug("[ RANK: %d; LAMPORT_CLOCK: %d; BLOOD_HUNGER: %d]", c.rank,
                  c.request.timestamp, c.request.bloodHunger)

            // If we didn't get a contract, increase blood_hunger and change
            // state to PEACE_IS_A_LIE
            if (my_contract_id == contractid_undefined) {
          debug("No work for me. Gonna rest a bit.") blood_hunger++;
          //          state = PEACE_IS_A_LIE;
          state = FINISH;
          break;
        }

        debug("Determined my contract id: %d", my_contract_id)

            // If we got a contract, send REQUEST_FOR_ARMOR to all gnomes with
            // contracts
            broadcast_rfa();

        debug("Gonna take some stuff from armoury.") state = TAKING_INVENTORY;
        break;
      }
      case TAKING_INVENTORY: {
        // If armory_queue is complete (gnome received REQUEST_FROM_ARMOR from
        // all gnomes)
        if (armory_queue.size() == contracts_num) {
          if (swords_needed < S && poison_kits_needed < P) {
            // Send ALLOCATE_ARMOR to other gnomes
            broadcast_aa();
            debug("I'm ready TO KILL!!!") state = RAMPAGE;
            break;
          }
          if (initiating_swap) {
            struct DelegatePriority dp_msg;
            comm_world.send(dp_msg, rank_to_delegate, DELEGATE_PRIORITY);

            const auto &status =
                comm_world.probe(mpl::any_source, mpl::tag::any());

            switch (static_cast<int>(status.tag())) {
              case SWAP:
              case ALLOCATE_ARMOR: {
                // If gnome probed SWAP or ALLOCATE_ARMOR from gnome he is
                // delegating to, move from initializing to swapping phase
                if (status.tag() == ALLOCATE_ARMOR) {
                  // Dummy receive
                  struct AllocateArmor dummy;
                  comm_world.recv(dummy, status.source(), ALLOCATE_ARMOR);
                }

                /*FIXME: Teoretycznie w nastepnej iteracji skrzat powinien
                 * za'probe'bować ten sam SWAP w innym switchu i tam go obsłużyć
                 */
                if (status.source() == rank_to_delegate)
                  initiating_swap = false;
              } break;
              case DELEGATE_PRIORITY: {
                // Dummy receive
                struct DelegatePriority dummy;
                comm_world.recv(dummy, status.source(), DELEGATE_PRIORITY);
              } break;
              default: {
                // Should never reach here
                //                debug("Entered superposition state. Committing
                //                suicide.") return;
              }
            }
          }
        }

        const auto &status = comm_world.probe(mpl::any_source, mpl::tag::any());

        switch (static_cast<int>(status.tag())) {
          case REQUEST_FOR_ARMOR: {
            receive_rfa(status.source());
            // If armory_queue is complete, calculate swords_needed and
            // poison_kits_needed.
            if (armory_queue.size() == contracts_num) {
              std::sort(armory_queue.begin(), armory_queue.end());
              for (auto swap_msg : swap_queue) {
                auto swap1 = aa_queue_find_position(swap_msg.delegatingRank);
                auto swap2 = aa_queue_find_position(swap_msg.delegatedRank);
                std::swap(swap1, swap2);
              }
              // Print armory_queue
              debug("Armory queue:") for (const auto &item : armory_queue)
                  debug(
                      "[ CLOCK: %2d; RANK: %2d; COMPLETED: %s; CONTRACT_ID: "
                      "%2d; NUM_HAMSTERS: %2d ]",
                      item.request.timestamp, item.rank,
                      (is_completed[item.request.contractId]) ? "true"
                                                              : "false",
                      item.request.contractId,
                      contracts[item.request.contractId].numberOfHamsters)

                  // Get my position in armory_queue
                  my_aaq_pos = aa_queue_find_position(rank);

              // Calculate variables
              swords_needed = std::distance(armory_queue.begin(), my_aaq_pos);
              poison_kits_needed = std::accumulate(
                  armory_queue.begin(), my_aaq_pos, 0,
                  [&](int sum, const auto &item) {
                    return (is_completed[item.request.contractId])
                               ? sum
                               : sum + contracts[item.request.contractId]
                                           .numberOfHamsters;
                  });

              const int my_pos_idx =
                  std::distance(armory_queue.begin(), my_aaq_pos);
              debug(
                  "My position in armory_queue = %d, swords_needed = %d, "
                  "poison_kits_needed = %d",
                  my_pos_idx, swords_needed, poison_kits_needed)

                  // Check if can swap with somebody
                  if (poison_kits_needed > P &&
                      my_aaq_pos != std::prev(armory_queue.end())) {
                debug("Checking if I can swap with somebody")

                    rank_to_delegate = get_rank_to_delegate();
                if (rank_to_delegate != -1) {
                  initiating_swap = true;
                  struct DelegatePriority dp_msg;
                  comm_world.send(dp_msg, rank_to_delegate, DELEGATE_PRIORITY);
                  break;
                }
              }
            }
            break;
          }
          case ALLOCATE_ARMOR: {
            debug("Received ALLOCATE ARMOR from GNOME %d",
                  status.source()) struct AllocateArmor aa_msg {};
            // Dummy receive
            comm_world.recv(aa_msg, status.source(), ALLOCATE_ARMOR);
            ranks_in_rampage[status.source()] = true;
            break;
          }
          case CONTRACT_COMPLETED: {
            struct ContractCompleted cc_msg {};
            comm_world.recv(cc_msg, status.source(), CONTRACT_COMPLETED);
            debug("Received CONTRACT_COMPLETED from GNOME %d.",
                  status.source()) if (armory_queue.size() < contracts_num) {
              is_completed[cc_msg.contractId] = true;
              continue;
            }
            swords_needed--;
            poison_kits_needed -= contracts[cc_msg.contractId].numberOfHamsters;
          } break;
          case DELEGATE_PRIORITY: {
            // Send SWAP to all gnomes between me and delegating gnome
            struct DelegatePriority dummy;
            comm_world.recv(dummy, status.source(), DELEGATE_PRIORITY);
            const auto delegating_pos = aa_queue_find_position(status.source());
            for (auto it = delegating_pos; it != my_aaq_pos; ++it) {
              struct Swap swap_msg {};
              swap_msg.timestamp = lamport_clock;
              swap_msg.delegatingRank = status.source();
              swap_msg.delegatedRank = rank;
              comm_world.send(swap_msg, it->rank, SWAP);
            }
            state = RAMPAGE;
          } break;
          case SWAP: {
            struct Swap swap_msg;
            comm_world.recv(swap_msg, status.source(), SWAP);
            handle_swap(swap_msg);
          } break;
        }
      } break;
      case RAMPAGE: {
        // Sleep time proportional to number of hamsters to kill. *fairness
        // noises*
        usleep(contracts[my_contract_id].numberOfHamsters * 1e5);
        debug(
            "Wildly murdered %d hamsters and completed my contract "
            "(CONTRACT_ID: %d).",
            contracts[my_contract_id].numberOfHamsters, my_contract_id)
            debug("Broadcasting CONTRACT_COMPLETE") broadcast_cc();

        blood_hunger = 0;
        state = FINISH;
        break;
      }
      default: {
        // Should never reach here
        debug("Entered superposition state. Committing suicide.") return;
      }
    }  // switch(state)
  }    // while(state != FINISH)
  debug("No work left for brave warrior. Committing suicide.")
}

void gnome::broadcast_cc() const {
  struct ContractCompleted cc_msg;
  cc_msg.timestamp = lamport_clock;
  cc_msg.contractId = my_contract_id;
  for (int dst_rank = 0; dst_rank < size; ++dst_rank) {
    if (dst_rank == rank) continue;
    comm_world.send(cc_msg, dst_rank, CONTRACT_COMPLETED);
  }
}

std::vector<struct ArmoryAllocationItem>::iterator
gnome::aa_queue_find_position(const int rank_) {
  return std::find_if(armory_queue.begin(), armory_queue.end(),
                      [=](const auto &item) { return item.rank == rank_; });
}

int gnome::get_contracts_from_landlord() {
  debug("Looking forward for new contracts") auto status =
      comm_world.probe(landlord_rank, CONTRACTS);
  contracts_num = status.get_count<Contract>();

  if (contracts_num != mpl::undefined) {
    contracts.resize(contracts_num);
  } else {
    // Should never reach here.
    debug("Invalid contracts number. Committing suicide.")
  }

  // Receive contracts from landlord
  comm_world.recv(contracts.begin(), contracts.end(), landlord_rank, CONTRACTS);
  debug("Received contract list.") return contracts_num;
}

void gnome::broadcast_rfc() {
  debug(
      "Broadcasting REQUEST_FOR_CONTRACT to other gnomes") for (int dst_rank =
                                                                    gnome_first;
                                                                dst_rank < size;
                                                                ++dst_rank) {
    struct RequestForContract rfc_msg {};
    rfc_msg.timestamp = ++lamport_clock;
    rfc_msg.bloodHunger = blood_hunger;
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
  for (auto cq_it = std::next(contract_queue.begin());
       cq_it != contract_queue.end(); ++cq_it) {
    const auto &status =
        comm_world.recv(cq_it->request, mpl::any_source, REQUEST_FOR_CONTRACT);

    cq_it->rank = status.source();
    lamport_clock = std::max(lamport_clock, cq_it->request.timestamp) + 1;

    debug("Received REQUEST_FOR_CONTRACT [ LAMPORT_CLOCK: %d ] from GNOME %d",
          cq_it->request.timestamp, cq_it->rank)
  }
}

int gnome::get_contractid() {
  const auto last_contract_it =
      std::next(contract_queue.begin(), contracts_num);
  std::partial_sort(contract_queue.begin(), last_contract_it,
                    contract_queue.end());
  const auto position_in_queue_it = std::find_if(
      contract_queue.begin(), contract_queue.end(),
      [=](const ContractQueueItem &item) { return item.rank == rank; });
  if (position_in_queue_it > last_contract_it)
    return contractid_undefined;
  else
    return std::distance(contract_queue.begin(), position_in_queue_it);
}

void gnome::broadcast_rfa() {
  const auto last_contract_it =
      std::next(contract_queue.begin(), contracts_num);
  debug(
      "Broadcasting REQUEST_FOR_ARMOR to other gnomes") for (auto it =
                                                                 contract_queue
                                                                     .begin();
                                                             it <
                                                             last_contract_it;
                                                             ++it) {
    struct RequestForArmor rfa_msg {};
    rfa_msg.timestamp = ++lamport_clock;
    rfa_msg.contractId = my_contract_id;

    if (it->rank == rank) {
      armory_queue.emplace_back(rank, rfa_msg);
      continue;
    }

    //    debug("Sending REQUEST_FOR_ARMOR to GNOME %d", it->rank)
    comm_world.send(rfa_msg, it->rank, REQUEST_FOR_ARMOR);
  }
}

void gnome::broadcast_aa() {
  debug("There are enough stuff available.") debug(
      "Broadcasting ALLOCATE_ARMOR to other gnomes") for (int dst_rank = 1;
                                                          dst_rank <=
                                                          gnomes_num;
                                                          ++dst_rank) {
    if (dst_rank == rank) continue;
    //    debug("Sending ALLOCATE_ARMOR to GNOME %d", dst_rank)
    const struct AllocateArmor aa_msg {};
    comm_world.send(aa_msg, dst_rank, ALLOCATE_ARMOR);
  }
}

void gnome::receive_rfa(int source) {
  struct RequestForArmor rfa {};
  comm_world.recv(rfa, source, REQUEST_FOR_ARMOR);
  debug("Received REQUEST_FOR_ARMOR from GNOME %d", source)
      armory_queue.emplace_back(source, rfa);
}

int gnome::get_rank_to_delegate() {
  // If exceeding even without this gnome, nobody can fit
  if ((poison_kits_needed - contracts[my_contract_id].numberOfHamsters) >= P)
    return -1;
  int max_hamsters_fit =
      P - (poison_kits_needed - contracts[my_contract_id].numberOfHamsters);
  auto pos = std::find_if(
      std::next(my_aaq_pos), armory_queue.end(), [=](const auto &item) {
        return contracts[item.request.contractId].numberOfHamsters <=
               max_hamsters_fit;
      });
  if (pos == armory_queue.end())
    return -1;
  else {
    debug("Initiating swap with %d, he has %d hamsters to kill, and I have %d",
          pos->rank, contracts[pos->request.contractId].numberOfHamsters,
          contracts[my_contract_id]
              .numberOfHamsters) return std::distance(armory_queue.begin(),
                                                      pos);
  }
}

void gnome::handle_swap(const struct Swap swap_msg) {
  if (armory_queue.size() != contracts_num) {
    swap_queue.push_back(swap_msg);
    return;
  }
  std::vector<struct ArmoryAllocationItem>::iterator swap1_pos;
  if (swap_msg.delegatingRank == rank)
    swap1_pos = my_aaq_pos;
  else
    swap1_pos = std::find_if(
        armory_queue.begin(), armory_queue.end(),
        [=](const auto &item) { return item.rank == swap_msg.delegatingRank; });
  auto swap2_pos = std::find_if(
      swap1_pos, armory_queue.end(),
      [=](const auto &item) { return item.rank == swap_msg.delegatedRank; });
  if (swap1_pos == my_aaq_pos) {
    for (auto it = std::next(my_aaq_pos); it <= swap2_pos; ++it) {
      poison_kits_needed += contracts[it->request.contractId].numberOfHamsters;
    }
  }
  // TODO: warunek będze zawsze spełniony, jeśli SWAP wysyłany tylko do procesów
  // pomiędzy nami
  if (swap1_pos < my_aaq_pos && my_aaq_pos < swap2_pos) {
    poison_kits_needed -=
        contracts[swap1_pos->request.contractId].numberOfHamsters -
        contracts[swap2_pos->request.contractId].numberOfHamsters;
  }
  // TODO: TBH nie ma sensu bawić siœ z samą kolejką skoro wszystko sależy od
  // poison_kts_needed
  std::swap(swap1_pos, swap2_pos);
}
