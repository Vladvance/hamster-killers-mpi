#ifndef ARG_PARSER_H_
#define ARG_PARSER_H_

#include <string>
#include <vector>

struct Configuration {
  int maxRounds = 1;
  int minHamstersPerContract = 10;
  int maxHamstersPerContract = 20;
  int swordsTotal = 5;
  int poisonTotal = 30;
};

class ArgParser {
 private:
  static bool getValue(std::string key, int& value, std::vector<std::string> args);

 public:
  static Configuration parse(int argc, char** argv);
};

#endif  // ARG_PARSER_H_
