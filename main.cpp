#include <mpi.h>
#include <iostream>
#include <cstddef>
#include <vector>
#include <random>
#include <algorithm>

#include "Common.h"


using namespace std;

void init() {
    //create contract type
    int contract_block_lengths[2] = {1, 1};
    MPI_Aint contract_offsets[2] = {offsetof(contract, id),
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
    MPI_Aint rfa_offsets[2] = {offsetof(request_for_armor, clock),
                               offsetof(request_for_armor, contract_id)};
    MPI_Datatype rfa_types[2] = {MPI_INT, MPI_INT};
    MPI_Type_create_struct(2, rfa_block_lengths, rfa_offsets, rfa_types, &MPI_REQUEST_FOR_ARMOR_T);
    MPI_Type_commit(&MPI_REQUEST_FOR_ARMOR_T);

    //create swap type
    int swap_block_lengths[1] = {2};
    MPI_Aint swap_offsets[1] = {offsetof(swap_proc, indexes)};
    MPI_Datatype swap_types[1] = {MPI_INT};
    MPI_Type_create_struct(1, swap_block_lengths, swap_offsets, swap_types, &MPI_SWAP_T);
    MPI_Type_commit(&MPI_SWAP_T);

//    MPI_Type_commit(&MPI_ALLOCATE_ARMOR_T);
//    MPI_Type_commit(&MPI_CONTRACT_COMPETED_T);
//    MPI_Type_commit(&MPI_DELEGATE_PRIORITY_T);
}

void finalize() {
    MPI_Type_free(&MPI_CONTRACT_T);
    MPI_Type_free(&MPI_REQUEST_FOR_CONTRACT_T);
    //test
//    MPI_Type_free(&MPI_ALLOCATE_ARMOR_T);
//    MPI_Type_free(&MPI_CONTRACT_COMPETED_T);
//    MPI_Type_free(&MPI_DELEGATE_PRIORITY_T);
    //test
    MPI_Type_free(&MPI_REQUEST_FOR_ARMOR_T);
    MPI_Type_free(&MPI_SWAP_T);
}

void showMessage(int rank, int lamport_clock, const char* message) {
    printf("[%d (%2d)]: %s\n", rank, lamport_clock, message);
}

void landlord_routine() {


}

void elf_routine() {

}

int main(int argc, char **argv) {
    int size, rank;
    MPI::Init(argc, argv);
    init();

    size = MPI::COMM_WORLD.Get_size();
    rank = MPI::COMM_WORLD.Get_rank();

    //num of contracts equal num of processes, FOR TESTING PURPOSE
    int contract_num = size - 1;

    std::vector<struct contract> contract_vec(contract_num);

    if(rank == LANDLORD_RANK) { //if landlord
        std::uniform_int_distribution<int> rand_hamster_num(1, MAX_HUMSTERS_IN_CONTRACT);
        std::random_device gen;

        //generate random contracts
        for(int i = 0; i < contract_num; ++i) {
            contract_vec[i].id = i;
            contract_vec[i].num_hamsters = rand_hamster_num(gen);
        }

        //send contracts to elves
//        MPI_Bcast(contract_vec.data(), contract_num, MPI_CONTRACT_T, LANDLORD_RANK, MPI::COMM_WORLD);
        for (int dst_rank = 1; dst_rank < size; ++dst_rank) {
            MPI_Send(contract_vec.data(), contract_num, MPI_CONTRACT_T, dst_rank, CONTRACTS, MPI::COMM_WORLD);
        }
    } else { //if elf
        std::vector<struct contract_queue_item> contract_queue;
        contract_queue.reserve(size - 1);
        std::vector<struct request_for_armor> armory_allocation_queue;
        armory_allocation_queue.reserve(size - 1);
        int poison_needed = 0, swords_needed = 0;
        int state = PEACE_IS_A_LIE;
        const int A = NUM_OF_SWORDS;
        const int T = NUM_OF_POISON_KITS;
        MPI_Status status;
        int lamport_clock = 0;
        int blood_hunger = 0;

//        while(state != FINISH) {
            switch (state) {
                case PEACE_IS_A_LIE: {
                    //receive contract from landlord
                    //MPI_Bcast(contract_vec.data(), contract_num, MPI_CONTRACT_T, LANDLORD_RANK, MPI::COMM_WORLD);
                    MPI_Recv(contract_vec.data(), contract_num, MPI_CONTRACT_T, LANDLORD_RANK, CONTRACTS,
                             MPI::COMM_WORLD,
                             &status);

                    //DEBUG
                    std::string pretty_string("CONTRACT LIST: | ");
                    for (auto &c : contract_vec) {
                        pretty_string += std::to_string(c.id) + ":" + std::to_string(c.num_hamsters) + " | ";
                    }
                    cout << pretty_string << endl;
                    //DEBUG

                    //send REQUEST_FOR_CONTRACT to the rest of elves
                    struct request_for_contract rfc_msg{lamport_clock, blood_hunger};

                    for (int i = 1; i < size; ++i) {
                        if (i == rank) continue;
                        MPI_Send(&rfc_msg, 1, MPI_REQUEST_FOR_CONTRACT_T, i, REQUEST_FOR_CONTRACT, MPI::COMM_WORLD);
                    }
                    contract_queue.push_back({rank, {lamport_clock, blood_hunger}});
                    state = GATHER_PARTY;
                    break;
                }
                case GATHER_PARTY: {
                    while (contract_queue.size() != size - 1) {
                        struct contract_queue_item item{};
                        MPI_Recv(&(item.rfc), 1, MPI_REQUEST_FOR_CONTRACT_T, MPI_ANY_SOURCE, REQUEST_FOR_CONTRACT,
                                 MPI::COMM_WORLD, &status);
                        item.rank = status.MPI_SOURCE;
                        contract_queue.push_back(item);
                    }

                    auto last_contract_it = std::next(contract_queue.begin(), contract_num);
                    std::partial_sort(contract_queue.begin(), last_contract_it, contract_queue.end());
                    auto pos_in_queue = std::find_if(contract_queue.begin(), contract_queue.end(),
                                                     [&rank](const contract_queue_item &item) {
                                                         return item.rank == rank;
                                                     });


                    //if we didn't get contract, increase blood_hunger and change state to PEACE_IS_A_LIE
                    if (pos_in_queue > last_contract_it) {
                        blood_hunger++;
                        state = PEACE_IS_A_LIE;
                        break;
                    }

                    //if we got contract, send REQUEST_FOR_CONTRACT to all processes with contracts
                    int idx_in_queue = std::distance(contract_queue.begin(), pos_in_queue);
                    struct request_for_armor rfa{idx_in_queue, lamport_clock};
                    for (auto it = contract_queue.begin(); it <= last_contract_it; ++it) {
                        MPI_Send(&rfa, 1, MPI_REQUEST_FOR_ARMOR_T, it->rank, REQUEST_FOR_ARMOR, MPI::COMM_WORLD);
                    }
                    //add corresponding item to armory_allocation_queue;
                    armory_allocation_queue.push_back(rfa);

                    //init helper variables
                    poison_needed = std::accumulate(contract_vec.begin(), contract_vec.end(), 0,
                                                    [](int sum, const struct contract &curr) {
                                                        return sum + curr.num_hamsters;
                                                    });
                    swords_needed = contract_num;

                    state = TAKING_INVENTORY;
                    break;
                }
                case TAKING_INVENTORY: {
//                if(swords_needed <= A && poison_needed <= T) {
//                    for (auto elf_rank: elves_with_contracts) {
//                        MPI_Send(nullptr, 0, MPI_ALLOCATE_ARMOR_T, elf_rank, ALLOCATE_ARMOR, MPI::COMM_WORLD);
//                    }
//                    state = RAMPAGE;
//                    break;
//                }


                    bool isDone = false;
                    if (armory_allocation_queue.size() == contract_num) {
                        while (!isDone) {
                            int rank_to_delegate = 1;
                            //send DELEGATE_PRIORITY message
                            MPI_Send(nullptr, 0, MPI_DELEGATE_PRIORITY_T, rank_to_delegate, DELEGATE_PRIORITY,
                                     MPI::COMM_WORLD);
                            //check for all received messages
                            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI::COMM_WORLD, &status);
                            switch (status.MPI_TAG) {
                                case DELEGATE_PRIORITY:
                                    //dummy receive
                                    MPI_Recv(nullptr, 0, MPI_DELEGATE_PRIORITY_T, status.MPI_SOURCE, DELEGATE_PRIORITY,
                                             MPI::COMM_WORLD, &status);
                                    break;
                                case SWAP: {
                                    struct swap_proc swap{};
                                    MPI_Recv(&swap, 1, MPI_SWAP_T, status.MPI_SOURCE, SWAP, MPI::COMM_WORLD, &status);
                                    if (swap.indexes[1] == rank) isDone = true;
                                    break;
                                }
                                case ALLOCATE_ARMOR: {
                                    //dummy receive
                                    MPI_Recv(nullptr, 0, MPI_ALLOCATE_ARMOR_T, status.MPI_SOURCE, ALLOCATE_ARMOR,
                                             MPI::COMM_WORLD, &status);
                                    if (status.MPI_SOURCE == rank_to_delegate) isDone = true;
                                    break;
                                }
                                default:
                                    break;
                            }
                        }
                    }

                    isDone = false;
                    while (!isDone) {
                        //check for all received messages
                        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI::COMM_WORLD, &status);
                        switch (status.MPI_TAG) {
                            case REQUEST_FOR_ARMOR: {
                                struct request_for_armor rfa{};
                                MPI_Recv(&rfa, 1, MPI_REQUEST_FOR_ARMOR_T, status.MPI_SOURCE, REQUEST_FOR_ARMOR,
                                         MPI::COMM_WORLD, &status);
                                armory_allocation_queue.push_back(rfa);

                                //TODO:Complete
                            };
                        }
                    }

                    break;
                }
                case RAMPAGE: {

                    state = PEACE_IS_A_LIE;
                }
                    break;
                default:
                    std::cout << "Wszedłem w stan superpozycji. Próbuję wyjść." << std::endl;
                    exit(-1);
            }
//        }
    }

    finalize();
    MPI::Finalize();
}
