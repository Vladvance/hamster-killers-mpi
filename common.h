//
// Created by vladvance on 25.05.2020.
//

#ifndef MPI_HAMSTER_KILLERS_COMMON_H_
#define MPI_HAMSTER_KILLERS_COMMON_H_

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
#define MAX_HAMSTERS_IN_CONTRACTS 20
#define NUM_OF_POISON_KITS 50
#define NUM_OF_SWORDS 10

#define DEBUG

#ifdef DEBUG
#define debug(FORMAT,...) std::printf("%c[%d;%dm [%8s %d]: " FORMAT "%c[%d;%dm\n",  27,  (1+(rank/7))%2, 31+(6+rank)%7, (rank==0) ? "LANDLORD" : "ELF", rank, ##__VA_ARGS__, 27,0,37);
#else
#define debug(...) ;
#endif

#define P_WHITE printf("%c[%d;%dm",27,1,37);
#define P_BLACK printf("%c[%d;%dm",27,1,30);
#define P_RED printf("%c[%d;%dm",27,1,31);
#define P_GREEN printf("%c[%d;%dm",27,1,33);
#define P_BLUE printf("%c[%d;%dm",27,1,34);
#define P_MAGENTA printf("%c[%d;%dm",27,1,35);
#define P_CYAN printf("%c[%d;%d;%dm",27,1,36);
#define P_SET(X) printf("%c[%d;%dm",27,1,31+(6+X)%7);
#define P_CLR printf("%c[%d;%dm",27,0,37);

/* printf ale z kolorkami i automatycznym wyświetlaniem RANK. Patrz debug wyżej po szczegóły, jak działa ustawianie kolorków */
#define println(FORMAT, ...) printf("%c[%d;%dm [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, ##__VA_ARGS__, 27,0,37);

#endif //MPI_HAMSTER_KILLERS_COMMON_H_
