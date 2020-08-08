//
// Created by vladvance on 05.08.2020.
//

#ifndef MPI_TYPES_H_
#define MPI_TYPES_H_

#include <mpi.h>

extern MPI_Datatype MPI_CONTRACT_T;
extern MPI_Datatype MPI_REQUEST_FOR_CONTRACT_T;
extern MPI_Datatype MPI_REQUEST_FOR_ARMOR_T;
extern MPI_Datatype MPI_ALLOCATE_ARMOR_T;
extern MPI_Datatype MPI_CONTRACT_COMPETED_T;
extern MPI_Datatype MPI_DELEGATE_PRIORITY_T;
extern MPI_Datatype MPI_SWAP_T;

struct contract {
  int contract_id;
  int num_hamsters;
};

struct request_for_contract {
  int lamport_clock;
  int blood_hunger;
  bool operator<(const request_for_contract &rhs) const {
    return (blood_hunger == rhs.blood_hunger) ?
           (lamport_clock == rhs.lamport_clock) :
           (blood_hunger > rhs.blood_hunger);
  }
  bool operator==(const request_for_contract &rhs) const {
    return(blood_hunger == rhs.blood_hunger && lamport_clock == rhs.lamport_clock);
  }
};

struct contract_queue_item {
  int rank;
  struct request_for_contract rfc;
  bool operator<(const contract_queue_item &rhs) const {
    return (rfc == rhs.rfc) ?
           (rank < rhs.rank) :
           (rfc < rhs.rfc);
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

void init_types();
void finalize_types();

#endif //MPI_TYPES_H_
