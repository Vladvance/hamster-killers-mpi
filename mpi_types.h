//
// Created by vladvance on 05.08.2020.
//

#ifndef MPI_TYPES_H_
#define MPI_TYPES_H_

#include <mpl/mpl.hpp>

enum MessageType {
  CONTRACTS,
  REQUEST_FOR_CONTRACT,
  REQUEST_FOR_ARMOR,
  ALLOCATE_ARMOR,
  CONTRACT_COMPLETED,
  DELEGATE_PRIORITY,
  SWAP
};

struct MessageBase {
  int timestamp;
};

struct Contract : public MessageBase {
  int contractId;
  int numberOfHamsters;

  Contract() = default;
  Contract(int contractId, int numberOfHamsters)
      : contractId(contractId), numberOfHamsters(numberOfHamsters) {}
};

struct RequestForContract : public MessageBase {
  int bloodHunger;

  RequestForContract() = default;
  RequestForContract(int bloodHunger) : bloodHunger(bloodHunger) {}

  bool operator<(const RequestForContract &rhs) const {
    return (bloodHunger == rhs.bloodHunger) ? (timestamp < rhs.timestamp)
                                            : (bloodHunger < rhs.bloodHunger);
  }

  bool operator==(const RequestForContract &rhs) const {
    return (bloodHunger == rhs.bloodHunger && timestamp == rhs.timestamp);
  }
};

struct RequestForArmor : public MessageBase {
  int contractId;

  RequestForArmor() = default;
  RequestForArmor(int contractId) : contractId(contractId) {}

  bool operator<(const RequestForArmor &rhs) const {
    return timestamp < rhs.timestamp;
  }

  bool operator==(const RequestForArmor &rhs) const {
    return timestamp == rhs.timestamp;
  }
};

struct AllocateArmor : public MessageBase {};

struct DelegatePriority : public MessageBase {};

struct ContractCompleted : public MessageBase {
  int contractId;

  ContractCompleted() = default;
  ContractCompleted(int contractId) : contractId(contractId) {}
};

struct Swap : public MessageBase {
  int delegatingRank;
  int delegatedRank;

  Swap() = default;
  Swap(int delegatingRank, int delegatedRank)
      : delegatingRank(delegatingRank), delegatedRank(delegatedRank) {}
};

struct ContractQueueItem {
  int rank;
  RequestForContract request;

  bool operator<(const ContractQueueItem &rhs) const {
    return (request == rhs.request) ? (rank < rhs.rank)
                                    : (request < rhs.request);
  }

  bool operator==(const ContractQueueItem &rhs) const {
    return (rank == rhs.rank && request == rhs.request);
  }
};

struct ArmoryAllocationItem {
  int rank;
  struct RequestForArmor request;

  ArmoryAllocationItem() = default;
  ArmoryAllocationItem(const int rank, const RequestForArmor &request)
      : rank(rank), request(request) {}

  bool operator<(const ArmoryAllocationItem &rhs) const {
    return (request == rhs.request) ? rank < rhs.rank : request < rhs.request;
  }

  bool operator==(const ArmoryAllocationItem &rhs) const {
    return (rank == rhs.rank && request == rhs.request);
  }
};

namespace mpl {

template <>
class struct_builder<Contract> : public base_struct_builder<Contract> {
  struct_layout<Contract> layout_;

 public:
  struct_builder() : base_struct_builder() {
    Contract str;
    layout_.register_struct(str);
    layout_.register_element(str.timestamp);
    layout_.register_element(str.contractId);
    layout_.register_element(str.numberOfHamsters);
    define_struct(layout_);
  }
};

template <>
class struct_builder<RequestForContract>
    : public base_struct_builder<RequestForContract> {
  struct_layout<RequestForContract> layout_;

 public:
  struct_builder() : base_struct_builder() {
    RequestForContract str;
    layout_.register_struct(str);
    layout_.register_element(str.timestamp);
    layout_.register_element(str.bloodHunger);
    define_struct(layout_);
  }
};

template <>
class struct_builder<RequestForArmor>
    : public base_struct_builder<RequestForArmor> {
  struct_layout<RequestForArmor> layout_;

 public:
  struct_builder() : base_struct_builder() {
    RequestForArmor str;
    layout_.register_struct(str);
    layout_.register_element(str.timestamp);
    layout_.register_element(str.contractId);
    define_struct(layout_);
  }
};

template <>
class struct_builder<AllocateArmor>
    : public base_struct_builder<AllocateArmor> {
  struct_layout<AllocateArmor> layout_;

 public:
  struct_builder() : base_struct_builder() {
    AllocateArmor str{};
    layout_.register_struct(str);
    layout_.register_element(str.timestamp);
    define_struct(layout_);
  }
};

template <>
class struct_builder<DelegatePriority>
    : public base_struct_builder<DelegatePriority> {
  struct_layout<DelegatePriority> layout_;

 public:
  struct_builder() : base_struct_builder() {
    DelegatePriority str{};
    layout_.register_struct(str);
    layout_.register_element(str.timestamp);
    define_struct(layout_);
  }
};

template <>
class struct_builder<ContractCompleted>
    : public base_struct_builder<ContractCompleted> {
  struct_layout<ContractCompleted> layout_;

 public:
  struct_builder() : base_struct_builder() {
    ContractCompleted str{};
    layout_.register_struct(str);
    layout_.register_element(str.timestamp);
    layout_.register_element(str.contractId);
    define_struct(layout_);
  }
};

template <>
class struct_builder<Swap> : public base_struct_builder<Swap> {
  struct_layout<Swap> layout_;

 public:
  struct_builder() : base_struct_builder() {
    Swap str{};
    layout_.register_struct(str);
    layout_.register_element(str.timestamp);
    layout_.register_element(str.delegatingRank);
    layout_.register_element(str.delegatedRank);
    define_struct(layout_);
  }
};

template <>
class struct_builder<ContractQueueItem>
    : public base_struct_builder<ContractQueueItem> {
  struct_layout<ContractQueueItem> layout_;

 public:
  struct_builder() : base_struct_builder() {
    ContractQueueItem str;
    layout_.register_struct(str);
    layout_.register_element(str.rank);
    layout_.register_element(str.request);
    define_struct(layout_);
  }
};

template <>
class struct_builder<ArmoryAllocationItem>
    : public base_struct_builder<ArmoryAllocationItem> {
  struct_layout<ArmoryAllocationItem> layout_;

 public:
  struct_builder() : base_struct_builder() {
    ArmoryAllocationItem str{};
    layout_.register_struct(str);
    layout_.register_element(str.rank);
    layout_.register_element(str.request);
    define_struct(layout_);
  }
};
}  // namespace mpl

#endif  // MPI_TYPES_H_
