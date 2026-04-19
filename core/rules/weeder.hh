#ifndef WEEDER_HH
#define WEEDER_HH

#include <string>
#include <vector>

#include "header.hh"

using namespace std;

class Weeder
{
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

  struct AllowHit
  {
    std::string expression;
    int matched;
    int is_negative;
    std::string header_tag;
    std::string header_body;
    int normalized_subject;
  };

  struct DenyHit
  {
    std::string expression;
    int matched;
    int is_negative;
    std::string header_tag;
    std::string header_body;
    int normalized_subject;
  };
  
private:
  vector<string> msg_ids;

  int last_score;
  string last_decision;
  std::vector<ScoreHit> last_score_hits; 
  std::vector<AllowHit> last_allow_hits;
  std::vector<DenyHit> last_deny_hits;

  int check_duplicates (Header*);
  int check_maxlength (Header*) const;
  int check_allow_rules (Header*);
  int check_deny_rules (Header*);
  int check_scores (Header*);

public:

  Weeder () : last_score (0), last_decision ("pass") {}

  int is_weed (Header*);

  int final_score (void) const { return last_score; }
  const string& decision (void) const { return last_decision; }
  const std::vector<ScoreHit>& score_hits (void) const { return last_score_hits; }
  const std::vector<AllowHit>& allow_hits (void) const { return last_allow_hits; }
  const std::vector<DenyHit>& deny_hits (void) const { return last_deny_hits; }
};
#endif
