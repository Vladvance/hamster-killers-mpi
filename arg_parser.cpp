#include "arg_parser.h"

#include <algorithm>
#include <mpl/mpl.hpp>
#include <sstream>

bool tryParse(std::string value, int& result) {
  std::istringstream stream(value);
  if (stream >> result) {
    return true;
  }
  return false;
}

bool ArgParser::getValue(std::string key, int& value, std::vector<std::string> args) {
  auto position = std::find(args.begin(), args.end(), key);
  if (position != args.end()) {
    if ((++position) != args.end()) {
      return tryParse(*position, value);
    }
    value = 0;
    return true;
  }
  return false;
}

Configuration ArgParser::parse(int argc, char** argv) {
  Configuration configuration;
  std::vector<std::string> args(argv + 1, argv + argc);
  int value;
  if (getValue("-h", value, args)) {
    if (mpl::environment::comm_world().rank() == 0) {
      printf(
          "usage: mpirun -np N %s\n"
          "N                                 number of processes to run - 1 landlord and N-1 gnomes\n"
          "[-h]                              show this message and exit\n"
          "[-r MAX_ROUNDS]                   number of times to repeat the contract cycle\n"
          "[-l MIN_HAMSTERS_PER_CONTRACT]    minimal number of hamsters to kill per contract\n"
          "[-h MAX_HAMSTERS_PER_CONTRACT]    maximal number of hamsters to kill per contract\n"
          "[-s SWORDS_TOTAL]                 total number of swords available to gnomes\n"
          "[-p POISON_TOTAL]                 total number of poison kits available to gnomes\n",
          argv[0]);
    }
    exit(EXIT_SUCCESS);
  }
  if (getValue("-r", value, args)) {
    configuration.maxRounds = value;
  }
  if (getValue("-l", value, args)) {
    configuration.minHamstersPerContract = value;
  }
  if (getValue("-u", value, args)) {
    configuration.maxHamstersPerContract = value;
  }
  if (getValue("-s", value, args)) {
    configuration.swordsTotal = value;
  }
  if (getValue("-p", value, args)) {
    configuration.poisonTotal = value;
  }
  return configuration;
}
