#ifndef WEEDER_HH
#define WEEDER_HH

#include <string>
#include <vector>

#include "header.hh"

using namespace std;

class Weeder
{
private:
  vector<string> msg_ids;

  int last_score;
  string last_decision;

  int check_duplicates (Header*);
  int check_maxlength (Header*) const;
  int check_allow_rules (Header*) const;
  int check_deny_rules (Header*) const;
  int check_scores (Header*) const;

public:
  Weeder () : last_score (0), last_decision ("pass") {}

  int is_weed (Header*);

  int final_score (void) const { return last_score; }
  const string& decision (void) const { return last_decision; }
};

#endif
