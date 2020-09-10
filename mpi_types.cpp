#include "mpi_types.h"

std::ostream& operator<<(std::ostream& stream, const RequestForContract &str) {
  stream << "[ LCLOCK:" << str.timestamp << ", BLOOD_HUNGER: " << str.bloodHunger << " ]";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Contract &str) {
  stream << "[ CID:" << str.contractId << ", NUM_HAMSTERS: " << str.numberOfHamsters << " ]";
  return stream;
}
