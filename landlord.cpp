//
// Created by vladvance on 05.08.2020.
//

#include "landlord.h"

#include <random>

#include "mpi_types.h"

std::random_device randomDevice;
int randomInt(int min, int max) {
  return std::uniform_int_distribution<int>{min, max}(randomDevice);
}

const int Landlord::landlordRank = 0;

Landlord::Landlord(const mpl::communicator& communicator)
    : ProcessBase(communicator, "LANDLORD"),
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
        // should never reach here
        log("Entered superposition state. Committing suicide.");
        return;
      }
    }
  }
  log("My mission in this world completed. Committing suicide.");
}

void Landlord::doHire() {
  contracts.clear();
  // Number of contracts equal to number of gnomes, FOR TESTING PURPOSE
  int numberOfContracts = numberOfGnomes;
  // int numberOfContracts = randomInt(1, numberOfGnomes);

  // Generate random contracts
  for (int i = 0; i < numberOfContracts; ++i) {
    int numberOfHamsters =
        randomInt(minHamstersPerContract, maxHamstersPerContract);
    contracts.push_back(Contract(i, numberOfHamsters));
    log("I have new contract: [ ID: %d, NUM_HAMSTERS: %d ]", i,
        numberOfHamsters);
  }
  isCompleted.resize(numberOfContracts);
  std::fill(isCompleted.begin(), isCompleted.end(), false);
  log("Total num of contracts in this wave: %d", numberOfContracts);

  // Send contracts to gnomes
  log("Broadcasting contract list.");
  broadcastVector(contracts, CONTRACTS);

  state = READ_GANDHI;
}

void Landlord::doReadGandhi() {
  ContractCompleted message{};
  const auto& status = receiveAny(message, CONTRACT_COMPLETED);
  int contractId = message.contractId;
  isCompleted[contractId] = true;
  log("I was informed that GNOME %d has murdered all %d hamsters and so "
      "completed his contract (ID : %d)",
      status.source(), contracts[contractId].numberOfHamsters, contractId);

  if (std::all_of(isCompleted.begin(), isCompleted.end(),
                  [](bool completed) { return completed; })) {
    state = FINISH;
  }
}
