#ifndef PREFERENCES_HH
#define PREFERENCES_HH

#include <fstream>
#include <string>
#include <vector>

#include "defines.hh"
#include "filter.hh"
#include "score.hh"

using namespace std;

class Preferences
{
protected:
  ifstream prefs_stream;

  vector<Filter> allows;
  vector<Filter> denies;
  vector<Score>  scores;

  string prefs_file_name;
  string log_file_name;
  string headers_file_name;
  string headers_sqlite3_file_name;

  int  icase;
  bool norm;
  bool test;
  bool show_headers;
  bool del_duplicates;
  bool ret_status;
  bool _ignore_time_stamp;
  bool _skip_ssl_verify;

  int         high_score;
  unsigned    time_out_val;
  int         max_size;
  Size_score  size_score;
  int         max_size_friends;
  int         max_line_length;
  int         rreg_type;
  int         verbosity;
  int         negative_allows;
  int         negative_denies;
  int         negative_scores;

  bool verbosity_changed;
  bool test_changed;

public:
  Preferences ();
  static Preferences& Instance ();

  void init (void);
  void kill (void);

  bool open (const char*);
  bool load (void);

  void add_deny_rule (const char*, const char*, const char*);
  void add_allow_rule (const char*, const char*, const char*);
  void add_score (const char*, int, const char*, const char*);

  int neg_allows (void);
  int neg_denies (void);

  void   set_rc_file (const char*);
  string rc_file (void);

  void   set_log_file (const char*);
  string log_file (void);

  void set_verbose_level (int);
  int  verbose_level (void);

  void   set_headers_file (const char*);
  string headers_file (void);

  void   set_headers_sqlite3_file (const char*);
  string headers_sqlite3_file (void);
  bool   use_headers_sqlite3 (void);

  void set_default_case (const char*);
  int  default_case (void);

  void set_reg_type (const char*);
  int  reg_type (void);

  unsigned int time_out (void);
  void         set_time_out (unsigned int);

  bool delete_duplicates (void);
  void set_del_duplicates (const char*);

  int  max_size_allow (void);
  void set_max_size_allow (int);

  int  max_size_deny (void);
  void set_max_size_deny (int);

  Size_score max_size_score (void);
  void       set_max_size_score (int, int);

  int  highscore (void);
  void set_highscore (int);

  bool normal (void);
  void set_normal (const char*);

  bool test_mode (void);
  void set_test_mode (const char*);

  int  maxlength (void);
  void set_maxlength (int);

  bool ignore_time_stamp ();
  void set_ignore_time_stamp (bool = true);

  bool return_status (void);
  void set_return_status (bool);

  void set_skip_ssl_verify (bool);
  bool skip_ssl_verify (void);

  vector<Filter>* allow_filters (void);
  vector<Filter>* deny_filters (void);
  vector<Score>*  score_filters (void);
};

#endif

