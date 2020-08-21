#ifndef LANDLORD_ROUTINE_H
#define LANDLORD_ROUTINE_H

#include <mpl/mpl.hpp>
#include <random>

class landlord {
 public:
  explicit landlord(const mpl::communicator& comm_world) : comm_world(comm_world),
                                            rank(comm_world.rank()),
                                            size(comm_world.size()),
                                            gnomes_num(size - 1)
  {}

  void run();
 private:
  // Constants
  static const int gnome_first = 1;
  static const int min_hamsters_per_contract = 10;
  static const int max_hamsters_per_contract = 20;
  static const int lamport_clock = 0; // Dummy

  // Attributes
  const mpl::communicator& comm_world;
  const int size;
  const int rank;
  const int gnomes_num;
  int contracts_num {0};
  std::random_device gen;

  std::vector<struct contract> contracts;
  std::vector<bool> is_completed;
  void receive_all_cc();
};

#endif // LANDLORD_ROUTINE_H
