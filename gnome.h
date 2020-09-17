#ifndef GNOME_H_
#define GNOME_H_

#include "mpi_types.h"
#include "process_base.h"

struct ContractQueueItem {
  int rank;
  RequestForContract request;

  bool operator<(const ContractQueueItem& rhs) const {
    return (request == rhs.request) ? (rank < rhs.rank)
                                    : (request < rhs.request);
  }

  bool operator==(const ContractQueueItem& rhs) const {
    return (rank == rhs.rank && request == rhs.request);
  }
};

struct ArmoryAllocationItem {
  int rank;
  struct RequestForArmor request;

  ArmoryAllocationItem() = default;
  ArmoryAllocationItem(const int rank, const RequestForArmor& request)
      : rank(rank), request(request) {}

  bool operator<(const ArmoryAllocationItem& rhs) const {
    return (request == rhs.request) ? (rank < rhs.rank)
                                    : (request < rhs.request);
  }

  bool operator==(const ArmoryAllocationItem& rhs) const {
    return (rank == rhs.rank && request == rhs.request);
  }
};

class Gnome : public ProcessBase {
 private:
  enum GnomeState {
    PEACE_IS_A_LIE,
    GATHER_PARTY,
    TAKING_INVENTORY,
    DELEGATING_PRIORITY,
    RAMPAGE,
    FINISH
  };
  const int numberOfGnomes;

  GnomeState state;
  int bloodHunger;
  int swordsNeeded;
  int poisonNeeded;
  int minValidContractId;
  int currentContractId;
  int swapRank;
  std::vector<Contract> contracts;
  std::vector<ContractQueueItem> contractQueue;
  std::vector<ArmoryAllocationItem> armoryQueue;
  std::vector<ArmoryAllocationItem>::iterator positionInArmoryQueue;
  std::vector<Swap> swapQueue;

  void doPeaceIsALie();
  void doGatherParty();
  void doTakingInventory();
  void doDelegatingPriority();
  void doRampage();

  const Contract& getContractById(int id) const;
  std::vector<int> getAllGnomeRanks() const;
  std::vector<int> getEmployedGnomeRanks() const;
  bool getContract();
  int findSwapCandidate();
  void applySwap(const Swap& swap);

  void handleRequestForArmor(const MessageBase* message, const mpl::status& status);
  void handleContractCompleted(const MessageBase* message, const mpl::status& status);
  void handleSwap(const MessageBase* message, const mpl::status& status);
  void handleDelegatePriority(const mpl::status &status);

  void handleSwapDelegating(const MessageBase* message, const mpl::status& status);
  void handleAllocateArmorDelegating(const MessageBase* message, const mpl::status& status);

 public:
  static int swordsTotal;
  static int poisonTotal;

  explicit Gnome(const mpl::communicator& communicator);
  void run(int maxRounds) override;
};

#endif  // GNOME_H_
