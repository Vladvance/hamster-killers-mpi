#include <random>

#include "landlord.h"
#include "mpi_types.h"

std::random_device randomDevice;
int randomInt(int min, int max) {
  return std::uniform_int_distribution<int>{min, max}(randomDevice);
}

const int Landlord::landlordRank = 0;
int Landlord::minHamstersPerContract = 10;
int Landlord::maxHamstersPerContract = 20;

Landlord::Landlord(const mpl::communicator& communicator)
    : ProcessBase(communicator, "LANDLORD"),
      minValidContractId(0),
      numberOfGnomes(communicator.size() - 1) {}

void Landlord::run(int maxRounds) {
  log("I'm alive!");

  state = HIRE;
  int round = 0;

  while (round != maxRounds) {
    switch (state) {
      case HIRE: {
        doHire();
        break;
      }
      case READ_GANDHI: {
        doReadGandhi();
        break;
      }
      case FINISH: {
        round++;
        state = HIRE;
        break;
      }
      default: {
        // Should never reach here
        log("Entered superposition state. Committing suicide.");
        return;
      }
    }
  }
  log("My mission in this world completed. Committing suicide.");
}

void Landlord::doHire() {
  minValidContractId += contracts.size();
  contracts.clear();
  int numberOfContracts = randomInt(1, numberOfGnomes);

  // Generate random contracts
  for (int i = 0, contractId = minValidContractId; i < numberOfContracts; ++i) {
    int numberOfHamsters =
        randomInt(minHamstersPerContract, maxHamstersPerContract);
    log("I have new contract: [ ID: %d, NUM_HAMSTERS: %d ]", contractId, numberOfHamsters);
    contracts.emplace_back(contractId++, numberOfHamsters);
  }
  isCompleted.resize(numberOfContracts);
  std::fill(isCompleted.begin(), isCompleted.end(), false);
  log("Total number of contracts in this wave: %d", numberOfContracts);

  // Send contracts to gnomes
  log("Broadcasting contract list.");
  broadcastVector(contracts, CONTRACTS);

  state = READ_GANDHI;
}

void Landlord::doReadGandhi() {
  ContractCompleted message{};
  const auto& status = receiveAny(message, CONTRACT_COMPLETED);
  int contractId = message.contractId;
  isCompleted[contractId - minValidContractId] = true;
  log("I was informed that GNOME %d has murdered all %d hamsters and so completed his contract (ID : %d)",
      status.source(), contracts[contractId - minValidContractId].numberOfHamsters, contractId);

  if (std::all_of(isCompleted.begin(), isCompleted.end(), [](bool completed) { return completed; })) {
    state = FINISH;
  }
}
