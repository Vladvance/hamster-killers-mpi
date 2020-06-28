//
// Created by vladvance on 25.05.2020.
//

#ifndef MPI_HAMSTER_KILLERS_COMMON_H
#define MPI_HAMSTER_KILLERS_COMMON_H

typedef enum { HIRE, READ_GANDHI } landlord_state_t;
typedef enum { PEACE_IS_A_LIE, GATHER_PARTY, TAKING_INVENTORY, RAMPAGE, FINISH } elf_state_t;

#define CONTRACTS 1
#define REQUEST_FOR_CONTRACT 2
#define REQUEST_FOR_ARMOR 3
#define ALLOCATE_ARMOR 4
#define CONTRACT_COMPLETED 5
#define DELEGATE_PRIORITY 6
#define SWAP 7

#define LANDLORD_RANK 0
#define MAX_HUMSTERS_IN_CONTRACT 20
#define NUM_OF_POISON_KITS 20
#define NUM_OF_SWORDS 10

MPI_Datatype MPI_CONTRACT_T;
MPI_Datatype MPI_REQUEST_FOR_CONTRACT_T;
MPI_Datatype MPI_REQUEST_FOR_ARMOR_T;
MPI_Datatype MPI_ALLOCATE_ARMOR_T;
MPI_Datatype MPI_CONTRACT_COMPETED_T;
MPI_Datatype MPI_DELEGATE_PRIORITY_T;
MPI_Datatype MPI_SWAP_T;

struct contract {
  int contract_id;
  int num_hamsters;
};

struct request_for_contract {
  int lamport_clock;
  int blood_hunger;
  bool operator<(const request_for_contract &rhs) const {
    return (blood_hunger == rhs.blood_hunger) ?
           (lamport_clock < rhs.lamport_clock) :
           (blood_hunger > rhs.blood_hunger);
  }
};

struct contract_queue_item {
  int rank;
  struct request_for_contract rfc;
  bool operator<(const contract_queue_item &rhs) const {
    return rfc < rhs.rfc;
  }
};

struct request_for_armor {
  int lamport_clock;
  int contract_id;
  bool operator<(const request_for_armor &rhs) const {
    return lamport_clock < rhs.lamport_clock;
  }
};

struct armory_allocation_item {
  int rank;
  struct request_for_armor rfa;
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

#endif //MPI_HAMSTER_KILLERS_COMMON_H
