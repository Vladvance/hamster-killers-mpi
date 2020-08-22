//
// Created by vladvance on 05.08.2020.
//

#ifndef GNOME_ROUTINE_H_
#define GNOME_ROUTINE_H_

#include <mpl/mpl.hpp>
#include "mpi_types.h"

class gnome {
 public:
  explicit gnome(const mpl::communicator& comm_world) :
      comm_world(comm_world),
      rank(comm_world.rank()),
      size(comm_world.size()),
      gnomes_num(size - 1),
      contract_queue(gnomes_num),
      is_completed(gnomes_num),
      ranks_in_rampage(gnomes_num)
  {
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
  int lamport_clock {};
  int contracts_num {};
  int swords_needed {};
  int poison_kits_needed {};
  int blood_hunger {};
  int my_contract_id {};

  std::vector<struct contract> contracts;
  std::vector<struct contract_queue_item> contract_queue;
  std::vector<struct armory_allocation_item> armory_queue;
  std::vector<bool> is_completed;
  std::vector<bool> ranks_in_rampage;
  std::vector<struct swap_proc> swap_queue;
  std::vector<struct armory_allocation_item>::iterator my_aaq_pos;
//  std::priority_queue<struct armory_allocation_item> aa_queue;

  int get_contractid();
  void broadcast_rfc();
  void broadcast_rfa();
  void broadcast_aa();
  void receive_rfa(int source);
  void receive_all_rfc();
  int get_contracts_from_landlord();
  void broadcast_cc() const;
  void handle_swap(struct swap_proc swap_msg);
  int get_rank_to_delegate();
  std::vector<struct armory_allocation_item>::iterator aa_queue_find_position(int rank);
};

#endif //GNOME_ROUTINE_H_
