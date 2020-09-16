#include "gnome.h"

#include <unistd.h>

#include "landlord.h"
#include "mpi_types.h"

int Gnome::swordsTotal = 5;
int Gnome::poisonTotal = 30;

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
        // Should never reach here
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
  armoryQueue.emplace_back(rank, request);
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
             handleDelegatePriority(status);
           }},
          {ALLOCATE_ARMOR,
           [](const MessageBase *message, const mpl::status &status) {
             // Receive and skip
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
             // Receive and skip
           }}};

  receiveMultiTag(mpl::any_source, messageHandlers);
}

void Gnome::doRampage() {
  log("I'm ready TO KILL!!!");
  // Sleep time proportional to number of hamsters to kill, *fairness noises*
  usleep(contracts[currentContractId].numberOfHamsters * 1e5);
  log("Wildly murdered %d hamsters and completed my contract (CONTRACT_ID: %d).",
      contracts[currentContractId].numberOfHamsters, currentContractId);
  log("Broadcasting CONTRACT_COMPLETE");
  ContractCompleted message(currentContractId);
  broadcast(message, CONTRACT_COMPLETED);
  send(message, Landlord::landlordRank, CONTRACT_COMPLETED);

  bloodHunger = 0;
  state = FINISH;
}

std::vector<int> Gnome::getAllGnomeRanks() const {
  auto ranks = std::vector<int>(numberOfGnomes + 1);
  std::iota(ranks.begin(), ranks.end(), 0);
  ranks.erase(std::find(ranks.begin(), ranks.end(), Landlord::landlordRank));
  return ranks;
}

std::vector<int> Gnome::getEmployedGnomeRanks() {
  std::vector<int> ranks(contracts.size());
  for (int i = 0; i < contracts.size(); i++) {
    ranks[i] = contractQueue[i].rank;
  }
  return ranks;
}

bool Gnome::getContract() {
  auto firstUnemployed = contractQueue.begin() + contracts.size();
  std::partial_sort(contractQueue.begin(), firstUnemployed, contractQueue.end());
  auto myPosition = std::find_if(
      contractQueue.begin(), contractQueue.end(),
      [=](const ContractQueueItem &item) { return item.rank == rank; });

  log("Contract queue:");
  for (const auto &contract : contractQueue) {
    log("[ RANK: %d; LAMPORT_CLOCK: %d; BLOOD_HUNGER: %d ]",
        contract.rank, contract.request.timestamp, contract.request.bloodHunger);
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
      poisonTotal - (poisonNeeded - contracts[currentContractId].numberOfHamsters);
  // If exceeding even without this gnome, nobody can fit
  if (maxPoisonAvailable <= 0 || swordsNeeded > swordsTotal) {
    return rank;
  }
  auto swapWith = std::find_if(
      std::next(positionInArmoryQueue), armoryQueue.end(),
      [this, maxPoisonAvailable](const auto &item) {
        return contracts[item.request.contractId].numberOfHamsters <= maxPoisonAvailable;
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

void Gnome::handleRequestForArmor(const MessageBase *message, const mpl::status &status) {
  log("Received REQUEST_FOR_ARMOR from GNOME %d", status.source());
  auto &request = *static_cast<const RequestForArmor *>(message);
  ArmoryAllocationItem queueItem(status.source(), request);
  armoryQueue.push_back(queueItem);

  if ((*positionInArmoryQueue) < queueItem) {
    auto contractId = queueItem.request.contractId;
    swordsNeeded--;
    poisonNeeded -= contracts[contractId].numberOfHamsters;
    log("Updated my resource requirements: swords_needed = %d, poison_kits_needed = %d",
        swordsNeeded, poisonNeeded);
  }

  // If armory queue is complete, sort it and apply deferred swaps
  if (armoryQueue.size() == contracts.size()) {
    std::sort(armoryQueue.begin(), armoryQueue.end());
    for (auto swap : swapQueue) {
      applySwap(swap);
    }
    swapQueue.clear();
    // Print armory queue
    log("Armory queue:");
    for (const auto &item : armoryQueue) {
      log("[ CLOCK: %2d; RANK: %2d; CONTRACT_ID: %2d; NUM_HAMSTERS: %2d ]",
          item.request.timestamp, item.rank, item.request.contractId,
          contracts[item.request.contractId].numberOfHamsters);
    }

    positionInArmoryQueue =
        std::find_if(armoryQueue.begin(), armoryQueue.end(),
                     [=](const auto &item) { return item.rank == rank; });

    log("My position in armory_queue = %d, swords_needed = %d, poison_kits_needed = %d",
        std::distance(armoryQueue.begin(), positionInArmoryQueue), swordsNeeded, poisonNeeded);
  }
}

void Gnome::handleContractCompleted(const MessageBase *message, const mpl::status &status) {
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

void Gnome::handleDelegatePriority(const mpl::status &status) {
  log("Received DELEGATE_PRIORITY from GNOME %d.", status.source());
  auto swap = Swap(status.source(), rank);
  broadcast(swap, SWAP);
  state = RAMPAGE;
}

void Gnome::handleSwapDelegating(const MessageBase *message, const mpl::status &status) {
  log("Received SWAP from GNOME %d.", status.source());
  auto &swap = *static_cast<const Swap *>(message);
  applySwap(swap);
  if ((swap.delegatedRank == swapRank) || (swap.delegatingRank == swapRank)) {
    state = TAKING_INVENTORY;
  }
}

void Gnome::handleAllocateArmorDelegating(const MessageBase *message, const mpl::status &status) {
  log("Received ALLOCATE_ARMOR from GNOME %d.", status.source());
  if (status.source() == swapRank) {
    state = TAKING_INVENTORY;
  }
}
