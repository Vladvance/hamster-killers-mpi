#include "mpi_types.h"

std::ostream& operator<<(std::ostream& stream, const request_for_contract &str) {
  stream << "[ LCLOCK:" << str.lamport_clock << ", BLOOD_HUNGER: " << str.blood_hunger << " ]";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const contract &str) {
  stream << "[ CID:" << str.contract_id << ", NUM_HAMSTERS: " << str.num_hamsters << " ]";
  return stream;
}
