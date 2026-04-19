// preferences.cc - source file for the mailfilter program
// Copyright (c) 2000 - 2009  Andreas Bauer <baueran@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

#include <string>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstdio>
#include "preferences.hh"
#include "filter.hh"
#include "mailfilter.hh"
#include "score.hh"
#include "rcfile.hh"

extern "C"
{
#include <wordexp.h>
#include <sys/types.h>
#include <regex.h>
}

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

using namespace std;

extern "C"
{ int rcparse (void*); }

extern int cmp_no_case (const string&, const string&);

Preferences :: Preferences ()
{
  icase             = CASE_DEFAULT;
  norm              = false;
  test              = false;
  show_headers      = false;
  del_duplicates    = false;
  ret_status        = false;
  _ignore_time_stamp= false;
  _skip_ssl_verify  = false;
  high_score        = 100;
  time_out_val      = 30;
  negative_allows   = 0;
  negative_denies   = 0;
  negative_scores   = 0;
  max_size          = 0;
  max_size_friends  = 0;
  max_line_length   = 0;
  rreg_type         = 0;
  verbosity         = 3;
  // Flags indicate whether a value was touched before.  See comments
  // inside preferences.hh for further details.
  verbosity_changed = false;
  test_changed      = false;
}

Preferences& Preferences :: Instance ()
{
  static Preferences* instance = new Preferences;
  return *instance;
}

void Preferences :: init (void)
{
  size_score.score = 0;
  size_score.size = 0;
}

  
void Preferences :: kill (void)
{
  allows.clear();
  denies.clear();
  scores.clear();
}

void Preferences :: set_ignore_time_stamp(bool new_ts)
{
  _ignore_time_stamp = new_ts;
}

bool Preferences :: ignore_time_stamp()
{
  return _ignore_time_stamp;
}

// This function tries to locate a preferences file and, upon success,
// stores the file path in prefs_file.  However, no data is loaded by
// the open() function.
    
bool Preferences :: open (const char* name)
{
  if (name && *name)
    prefs_file_name = name;

  if (!prefs_file_name.length ())
    return false;

  prefs_stream.open (prefs_file_name.c_str ());

  if (!prefs_stream.is_open ())
    return false;

  return true;
}

// This function loads the user's preferences file which is specified
// in prefs_file.  This string, containing the path, must not be empty
// at this point.
/*
bool Preferences :: load (void)
{
  if (!prefs_stream.is_open ())
    return false;
  
  try
    {
      RCParser rcparser(&prefs_stream);
      rcparser.parse();
    }
  catch (...) { throw; }

//Anfang alle Regeln kompilieren
  for (auto &f : allows) {
    if (f.compile() != 0) {
      ERROR_MSG("Failed to compile allow rule regex: `" + f.expression() + "'.");
      return false;
    }
  }

  for (auto &f : denies) {
    if (f.compile() != 0) {
      ERROR_MSG("Failed to compile deny rule regex: `" + f.expression() + "'.");
      return false;
    }
  }

  for (auto &s : scores) {
    if (s.compile() != 0) {
      ERROR_MSG("Failed to compile score rule regex: `" + s.expression() + "'.");
      return false;
    }
  }
//Ende

  return true;
}
*/


// Anfang mit Debug

bool Preferences :: load (void)
{
  if (!prefs_stream.is_open ())
    return false;

  try
    {
      RCParser rcparser(&prefs_stream);
      rcparser.parse();

      std::fprintf(stderr,
                   "DEBUG prefs: parse rc=%d allows=%zu denies=%zu scores=%zu\n",
                   parse_rc,
                   allows.size(),
                   denies.size(),
                   scores.size());

      if (parse_rc != 0)
        {
          ERROR_MSG("RC parser returned non-zero status.");
          return false;
        }
    }
  catch (...) { throw; }

  for (auto &f : allows)
    {
      if (f.compile () != 0)
        {
          ERROR_MSG("Failed to compile allow rule regex: `" + f.expression () + "'.");
          return false;
        }
    }

  for (auto &f : denies)
    {
      if (f.compile () != 0)
        {
          ERROR_MSG("Failed to compile deny rule regex: `" + f.expression () + "'.");
          return false;
        }
    }

  for (auto &s : scores)
    {
      if (s.compile () != 0)
        {
          ERROR_MSG("Failed to compile score rule regex: `" + s.expression () + "'.");
          return false;
        }
    }

  return true;
}
// Ende inkl. Debug

void Preferences :: add_deny_rule (const char* keyword,
				   const char* operat,
				   const char* id)
{
// Debug
    std::fprintf(stderr,
             "DEBUG prefs: add_deny_rule keyword=%s oper=%s expr=%s\n",
             keyword ? keyword : "(null)",
             operat ? operat : "(null)",
             id ? id : "(null)");
// Ende Debug
  Filter cur_filter;

  if (strcmp (operat, "=") == 0)
    cur_filter.set_negativity (false);
  else
    {
      negative_denies++;
      cur_filter.set_negativity (true);
    }

  cur_filter.set_expression (id);

  if (cmp_no_case (keyword, "deny_case") == 0)
    cur_filter.set_case (CASE_SENSITIVE);
  else if (cmp_no_case (keyword, "deny_nocase") == 0)
    cur_filter.set_case (CASE_INSENSITIVE);
  else
    cur_filter.set_case (default_case ());

  denies.push_back (cur_filter);
}

void Preferences :: add_allow_rule (const char* keyword,
				    const char* operat,
				    const char* id)
{
// Debug
    std::fprintf(stderr,
             "DEBUG prefs: add_allow_rule keyword=%s oper=%s expr=%s\n",
             keyword ? keyword : "(null)",
             operat ? operat : "(null)",
             id ? id : "(null)");
// Ende Debug

  Filter cur_filter;

  if (strcmp (operat, "=") == 0)
    cur_filter.set_negativity (false);
  else
    {
      negative_allows++;
      cur_filter.set_negativity (true);
    }

  cur_filter.set_expression (id);

  if (cmp_no_case (keyword, "allow_case") == 0)
    cur_filter.set_case (CASE_SENSITIVE);
  else if (cmp_no_case (keyword, "allow_nocase") == 0)
    cur_filter.set_case (CASE_INSENSITIVE);
  else
    cur_filter.set_case (default_case ());

  allows.push_back (cur_filter);
}

void Preferences :: add_score (const char* keyword,
			       int given_score,
			       const char* operat,
			       const char* id)
{
  Score cur_score;
    
  if (strcmp (operat, "=") == 0)
    cur_score.set_negativity (false);
  else
    {
      negative_scores++;
      cur_score.set_negativity (true);
    }

  cur_score.set_expression (id);
  cur_score.set_score (given_score);

  if (cmp_no_case (keyword, "score_case") == 0)
    cur_score.set_case (CASE_SENSITIVE);
  else if (cmp_no_case (keyword, "score_nocase") == 0)
    cur_score.set_case (CASE_INSENSITIVE);
  else
    cur_score.set_case (default_case ());

  scores.push_back (cur_score);
}

void Preferences :: set_headers_file (const char* name)
{
  // Expand the given file name.
  wordexp_t result;
  if (wordexp (name, &result, 0) == 0 && result.we_wordc > 0) {
    headers_file_name = result.we_wordv[0];
  }
  else {
    ERROR_MSG("Invalid headers store-file name: `" + (string)name + "'.");
    exit(-1);
  }
  wordfree (&result);
}

string Preferences :: headers_file (void)
{ return headers_file_name; }

void Preferences :: set_headers_sqlite3_file (const char* name)
{
  wordexp_t result;

  if (wordexp (name, &result, 0) == 0 && result.we_wordc > 0)
    {
      headers_sqlite3_file_name = result.we_wordv[0];
    }
  else
    {
      ERROR_MSG("Invalid sqlite3 header log-file name: `"
                + (string)name + "'.");
      exit (-1);
    }

  wordfree (&result);
}

string Preferences :: headers_sqlite3_file (void)
{
  return headers_sqlite3_file_name;
}

bool Preferences :: use_headers_sqlite3 (void)
{
  return (headers_sqlite3_file_name.length () > 0);
}

void Preferences :: set_rc_file (const char* name)
{ prefs_file_name = name; }

string Preferences :: rc_file (void)
{ return prefs_file_name; }
  
void Preferences :: set_log_file (const char* name)
{
  // Only expand logfile name if none was already specified, eg on the
  // command line.
  if (log_file_name.length() == 0) {
    wordexp_t result;
    if (wordexp(name, &result, 0) == 0 && result.we_wordc > 0) {
      log_file_name = result.we_wordv[0];
    }
    else {
      ERROR_MSG("Invalid logfile name: `" + (string)name + "'.");
      exit(-1);
    }
    wordfree (&result);
  }
}

string Preferences :: log_file (void)
{ return log_file_name; }
  
void Preferences :: set_verbose_level (int level)
{ 
  if (!verbosity_changed)
    {
      verbosity = level;
      verbosity_changed = true;
    }
}

int Preferences :: verbose_level (void)
{ return verbosity; }

void Preferences :: set_default_case (const char* new_case)
{ icase = ((cmp_no_case (new_case, "yes") == 0) ? 0 : REG_ICASE); }

int Preferences :: default_case (void)
{ return icase; }

void Preferences :: set_reg_type (const char* new_type)
{ 
  if (cmp_no_case (new_type, "extended") == 0)
    rreg_type = REG_EXTENDED;
  else if (cmp_no_case (new_type, "basic") == 0)
    rreg_type = 0;
  else
    {
      ERROR_MSG((string)"Regular expressions must either be of type "
		+ (string)"'extended', or 'basic'.");
      exit (-1);
    }
}

int Preferences :: reg_type (void)
{ return rreg_type; }

void Preferences :: set_server (const char*) {}
void Preferences :: set_usr (const char*) {}
void Preferences :: set_passwd (const char*) {}
void Preferences :: set_protocol (const char*) {}
void Preferences :: set_connection (unsigned int) {}
void Preferences :: set_port (unsigned int) {}

bool Preferences :: delete_duplicates (void)
{  return del_duplicates; }

void Preferences :: set_del_duplicates (const char* del)
{ del_duplicates = (cmp_no_case (del, "yes") == 0 ? true : false); }

vector<Filter>* Preferences :: allow_filters (void)
{ return &allows; }

vector<Filter>* Preferences :: deny_filters (void)
{ return &denies; }

vector<Score>* Preferences :: score_filters (void)
{ return &scores; }

void Preferences :: set_time_out (unsigned int val)
{ time_out_val = val; }

unsigned int Preferences :: time_out (void)
{ return time_out_val; }

int Preferences :: max_size_allow (void)
{ return max_size_friends; }

void Preferences :: set_max_size_allow (int val)
{ max_size_friends = val; }

int Preferences :: max_size_deny (void)
{ return max_size; }

void Preferences :: set_max_size_deny (int val)
{ max_size = val; }

Size_score Preferences :: max_size_score (void)
{ return size_score; }

void Preferences :: set_max_size_score (int score,
					int size)
{ 
  size_score.score = score;
  size_score.size = size;
}

int Preferences :: neg_allows (void)
{ return negative_allows; }

int Preferences :: neg_denies (void)
{ return negative_denies; }

int Preferences :: highscore (void)
{ return high_score; }

void Preferences :: set_highscore (int val)
{ high_score = val; }

void Preferences :: set_normal (const char* par)
{ norm = (cmp_no_case (par, "yes") == 0 ? true : false); }

bool Preferences :: normal (void)
{ return norm; }

bool Preferences :: test_mode (void)
{ return test; }

void Preferences :: set_test_mode (const char* par)
{
  if (!test_changed)
    {
      test = (cmp_no_case (par, "yes") == 0 ? true : false);
		       test_changed = true;
    }
}

void Preferences :: set_maxlength (int val)
{ max_line_length = val; }

int Preferences :: maxlength (void)
{ return max_line_length; }

bool Preferences :: return_status (void)
{ return ret_status; }

void Preferences :: set_return_status (bool st)
{ ret_status = st; }

void Preferences :: set_skip_ssl_verify (bool skip)
{ _skip_ssl_verify = skip; }

bool Preferences :: skip_ssl_verify (void)
{ return _skip_ssl_verify; }
