#ifndef GNOME_H_
#define GNOME_H_

#include "process_base.h"

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
  int currentContractId;
  int swapRank;
  std::vector<struct Contract> contracts;
  std::vector<struct ContractQueueItem> contractQueue;
  std::vector<struct ArmoryAllocationItem> armoryQueue;
  std::vector<struct ArmoryAllocationItem>::iterator positionInArmoryQueue;
  std::vector<struct Swap> swapQueue;

  void doPeaceIsALie();
  void doGatherParty();
  void doTakingInventory();
  void doDelegatingPriority();
  void doRampage();

  std::vector<int> getAllGnomeRanks();
  std::vector<int> getEmployedGnomeRanks();
  bool getContract();
  int findSwapCandidate();
  void applySwap(const Swap& swap);

  void handleRequestForArmor(const MessageBase* message,
                             const mpl::status& status);
  void handleContractCompleted(const MessageBase* message,
                               const mpl::status& status);
  void handleSwap(const MessageBase* message, const mpl::status& status);
  void handleDelegatePriority(const MessageBase* message,
                              const mpl::status& status);

  void handleSwapDelegating(const MessageBase* message,
                            const mpl::status& status);
  void handleAllocateArmorDelegating(const MessageBase* message,
                                     const mpl::status& status);

 public:
  static const int swordsTotal = 5;
  static const int poisonTotal = 20;

  Gnome(const mpl::communicator& communicator);
  virtual void run(int maxRounds) override;
};

#include <mpl/mpl.hpp>

#include "mpi_types.h"

class gnome {
 public:
  explicit gnome(const mpl::communicator& comm_world)
      : comm_world(comm_world),
        rank(comm_world.rank()),
        size(comm_world.size()),
        gnomes_num(size - 1),
        contract_queue(gnomes_num),
        is_completed(gnomes_num),
        ranks_in_rampage(gnomes_num) {
    armory_queue.reserve(gnomes_num);
  }
  void run();

 private:
  // Constants
  static const int gnome_first = 1;
  static const int S = 10;
  static const int P = 20;

  const mpl::communicator& comm_world;

  const int rank;
  const int size;
  const int gnomes_num;
  int lamport_clock{};
  int contracts_num{};
  int swords_needed{};
  int poison_kits_needed{};
  int blood_hunger{};
  int my_contract_id{};

  std::vector<struct Contract> contracts;
  std::vector<struct ContractQueueItem> contract_queue;
  std::vector<struct ArmoryAllocationItem> armory_queue;
  std::vector<bool> is_completed;
  std::vector<bool> ranks_in_rampage;
  std::vector<struct Swap> swap_queue;
  std::vector<struct ArmoryAllocationItem>::iterator my_aaq_pos;
  //  std::priority_queue<struct armory_allocation_item> aa_queue;

  int get_contractid();
  void broadcast_rfc();
  void broadcast_rfa();
  void broadcast_aa();
  void receive_rfa(int source);
  void receive_all_rfc();
  int get_contracts_from_landlord();
  void broadcast_cc() const;
  void handle_swap(struct Swap swap_msg);
  int get_rank_to_delegate();
  std::vector<struct ArmoryAllocationItem>::iterator aa_queue_find_position(
      int rank);
};

#endif  // GNOME_H_
