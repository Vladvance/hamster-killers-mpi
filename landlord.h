#ifndef LANDLORD_H_
#define LANDLORD_H_

#include "process_base.h"

class Landlord : public ProcessBase {
 private:
  enum LandlordState { HIRE, READ_GANDHI, FINISH };
  const int numberOfGnomes;

  LandlordState state;
  std::vector<struct Contract> contracts;
  std::vector<bool> isCompleted;

  void doHire();
  void doReadGandhi();

 public:
  static const int landlordRank;
  static const int minHamstersPerContract = 10;
  static const int maxHamstersPerContract = 20;

  Landlord(const mpl::communicator& communicator);
  virtual void run(int maxRounds) override;
};

#endif  // LANDLORD_H_
