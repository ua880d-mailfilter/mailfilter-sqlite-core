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
  int check_scores (Header*);

  std::vector<ScoreHit> last_score_hits;

public:


  struct ScoreHit
  {
    std::string expression;
    int score_delta;
    int is_negative;
    int matched;
    std::string header_tag;
    std::string header_body;
    int normalized_subject;
  };

  Weeder () : last_score (0), last_decision ("pass") {}

  int is_weed (Header*);

  int final_score (void) const { return last_score; }
  const string& decision (void) const { return last_decision; }
  const std::vector<ScoreHit>& score_hits (void) const { return last_score_hits; }
};

#endif
