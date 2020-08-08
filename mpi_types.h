//
// Created by vladvance on 05.08.2020.
//

#ifndef MPI_TYPES_H_
#define MPI_TYPES_H_

#include <mpl/mpl.hpp>


struct contract {
  int contract_id{};
  int num_hamsters{};

  contract(int contract_id, int num_hamsters)
  : contract_id(contract_id), num_hamsters(num_hamsters) {}
};

std::ostream& operator<<(std::ostream& stream, const contract &str) {
  stream << "[ CID:" << str.contract_id << ", NUM_HAMSTERS: " << str.num_hamsters << " ]";
  return stream;
}

struct request_for_contract {
  int lamport_clock = 0;
  int blood_hunger = 0;

  request_for_contract(int lamport_clock, int blood_hunger)
      : lamport_clock(lamport_clock), blood_hunger(blood_hunger) {}

  bool operator<(const request_for_contract &rhs) const {
    return (blood_hunger == rhs.blood_hunger) ?
           (lamport_clock == rhs.lamport_clock) :
           (blood_hunger > rhs.blood_hunger);
  }

  bool operator==(const request_for_contract &rhs) const {
    return(blood_hunger == rhs.blood_hunger && lamport_clock == rhs.lamport_clock);
  }

};

std::ostream& operator<<(std::ostream& stream, const request_for_contract &str) {
  stream << "[ LCLOCK:" << str.lamport_clock << ", BLOOD_HUNGER: " << str.blood_hunger << " ]";
  return stream;
}

struct contract_queue_item {
  int rank{};
  struct request_for_contract rfc;

  contract_queue_item(int rank, const request_for_contract &rfc) : rank(rank), rfc(rfc) {}

  bool operator<(const contract_queue_item &rhs) const {
    return (rfc == rhs.rfc) ?
           (rank < rhs.rank) :
           (rfc < rhs.rfc);
  }

};

struct request_for_armor {
  int lamport_clock{};
  int contract_id{};

  request_for_armor(int lamport_clock, int contract_id) : lamport_clock(lamport_clock), contract_id(contract_id) {}

  bool operator<(const request_for_armor &rhs) const {
    return lamport_clock < rhs.lamport_clock;
  }
};

struct armory_allocation_item {
  int rank{};
  struct request_for_armor rfa{};

  armory_allocation_item(int rank, const request_for_armor &rfa) : rank(rank), rfa(rfa) {}

  bool operator<(const armory_allocation_item &rhs) const {
    return rfa < rhs.rfa;
  }
};

struct contract_completed {
  int contract_id;
};

struct swap_proc {
  int rank_who_delegates;
  int rank_to_whom_delegated;
};

namespace mpl {

template<>
class struct_builder<contract> : public base_struct_builder<contract> {
  struct_layout<contract> layout_;
 public:
  struct_builder() : base_struct_builder() {
    contract str;
    layout_.register_struct(str);
    layout_.register_element(str.contract_id);
    layout_.register_element(str.num_hamsters);
    define_struct(layout_);
  }
};

template<>
class struct_builder<request_for_contract> : public base_struct_builder<request_for_contract> {
  struct_layout<request_for_contract> layout_;
 public:
  struct_builder() : base_struct_builder() {
    request_for_contract str;
    layout_.register_struct(str);
    layout_.register_element(str.lamport_clock);
    layout_.register_element(str.blood_hunger);
    define_struct(layout_);
  }
};

template<>
class struct_builder<contract_queue_item> : public base_struct_builder<contract_queue_item> {
  struct_layout<contract_queue_item> layout_;
 public:
  struct_builder() : base_struct_builder() {
    contract_queue_item str;
    layout_.register_struct(str);
    layout_.register_element(str.rank);
    layout_.register_element(str.rfc);
    define_struct(layout_);
  }
};

template<>
class struct_builder<request_for_armor> : public base_struct_builder<request_for_armor> {
  struct_layout<request_for_armor> layout_;
 public:
  struct_builder() : base_struct_builder() {
    request_for_armor str;
    layout_.register_struct(str);
    layout_.register_element(str.lamport_clock);
    layout_.register_element(str.contract_id);
    define_struct(layout_);
  }
};

template<>
class struct_builder<armory_allocation_item> : public base_struct_builder<armory_allocation_item> {
  struct_layout<armory_allocation_item> layout_;
 public:
  struct_builder() : base_struct_builder() {
    armory_allocation_item str;
    layout_.register_struct(str);
    layout_.register_element(str.rank);
    layout_.register_element(str.rfa);
    define_struct(layout_);
  }
};

template<>
class struct_builder<contract_completed> : public base_struct_builder<contract_completed> {
  struct_layout<contract_completed> layout_;
 public:
  struct_builder() : base_struct_builder() {
    contract_completed str{};
    layout_.register_struct(str);
    layout_.register_element(str.contract_id);
    define_struct(layout_);
  }
};

template<>
class struct_builder<swap_proc> : public base_struct_builder<swap_proc> {
  struct_layout<swap_proc> layout_;
 public:
  struct_builder() : base_struct_builder() {
    swap_proc str{};
    layout_.register_struct(str);
    layout_.register_element(str.rank_who_delegates);
    layout_.register_element(str.rank_to_whom_delegated);
    define_struct(layout_);
  }
};

}

#endif //MPI_TYPES_H_
