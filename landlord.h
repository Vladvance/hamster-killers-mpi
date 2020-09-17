#ifndef LANDLORD_H_
#define LANDLORD_H_

#include "mpi_types.h"
#include "process_base.h"

class Landlord : public ProcessBase {
 private:
  enum LandlordState { HIRE, READ_GANDHI, FINISH };
  const int numberOfGnomes;

  LandlordState state;
  std::vector<Contract> contracts;
  std::vector<bool> isCompleted;

  void doHire();
  void doReadGandhi();

 public:
  static const int landlordRank;
  static int minHamstersPerContract;
  static int maxHamstersPerContract;

  explicit Landlord(const mpl::communicator& communicator);
  void run(int maxRounds) override;
};

#endif  // LANDLORD_H_
