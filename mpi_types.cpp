//
// Created by vladvance on 05.08.2020.
//

#include "mpi_types.h"
#include <mpi.h>

MPI_Datatype MPI_CONTRACT_T;
MPI_Datatype MPI_REQUEST_FOR_CONTRACT_T;
MPI_Datatype MPI_REQUEST_FOR_ARMOR_T;
MPI_Datatype MPI_ALLOCATE_ARMOR_T;
MPI_Datatype MPI_CONTRACT_COMPETED_T;
MPI_Datatype MPI_DELEGATE_PRIORITY_T;
MPI_Datatype MPI_SWAP_T;

void init_types() {
  //create contract type
  int contract_block_lengths[2] = {1, 1};
  MPI_Aint contract_offsets[2] = {offsetof(contract, contract_id),
                                  offsetof(contract, num_hamsters)};
  MPI_Datatype contract_types[2] = {MPI_INT, MPI_INT};
  MPI_Type_create_struct(2, contract_block_lengths, contract_offsets, contract_types, &MPI_CONTRACT_T);
  MPI_Type_commit(&MPI_CONTRACT_T);

  //create request_for_contract type
  int rfc_block_lengths[2] = {1, 1};
  MPI_Aint rfc_offsets[2] = {offsetof(request_for_contract, lamport_clock),
                             offsetof(request_for_contract, blood_hunger)};
  MPI_Datatype rfc_types[2] = {MPI_INT, MPI_INT};
  MPI_Type_create_struct(2, rfc_block_lengths, rfc_offsets, rfc_types, &MPI_REQUEST_FOR_CONTRACT_T);
  MPI_Type_commit(&MPI_REQUEST_FOR_CONTRACT_T);

  //create request_for_armor type
  int rfa_block_lengths[2] = {1, 1};
  MPI_Aint rfa_offsets[2] = {offsetof(request_for_armor, lamport_clock),
                             offsetof(request_for_armor, contract_id)};
  MPI_Datatype rfa_types[2] = {MPI_INT, MPI_INT};
  MPI_Type_create_struct(2, rfa_block_lengths, rfa_offsets, rfa_types, &MPI_REQUEST_FOR_ARMOR_T);
  MPI_Type_commit(&MPI_REQUEST_FOR_ARMOR_T);

  //create contract_completed type
  int cc_block_lengths[1] = {1};
  MPI_Aint cc_offsets[1] = {offsetof(contract_completed, contract_id)};
  MPI_Datatype cc_types[1] = {MPI_INT};
  MPI_Type_create_struct(1, cc_block_lengths, cc_offsets, cc_types, &MPI_CONTRACT_COMPETED_T);
  MPI_Type_commit(&MPI_CONTRACT_COMPETED_T);

  //create swap type
  int swap_block_lengths[2] = {1, 1};
  MPI_Aint swap_offsets[2] = {offsetof(swap_proc, rank_who_delegates),
                              offsetof(swap_proc, rank_to_whom_delegated)};
  MPI_Datatype swap_types[2] = {MPI_INT, MPI_INT};
  MPI_Type_create_struct(1, swap_block_lengths, swap_offsets, swap_types, &MPI_SWAP_T);
  MPI_Type_commit(&MPI_SWAP_T);

//  MPI_Type_commit(&MPI_ALLOCATE_ARMOR_T);
//  MPI_Type_commit(&MPI_DELEGATE_PRIORITY_T);
}
void finalize_types() {
  MPI_Type_free(&MPI_CONTRACT_T);
  MPI_Type_free(&MPI_REQUEST_FOR_CONTRACT_T);
  MPI_Type_free(&MPI_CONTRACT_COMPETED_T);
  MPI_Type_free(&MPI_REQUEST_FOR_ARMOR_T);
  MPI_Type_free(&MPI_SWAP_T);
  //test
  MPI_Type_free(&MPI_ALLOCATE_ARMOR_T);
  MPI_Type_free(&MPI_DELEGATE_PRIORITY_T);
  //test
}
