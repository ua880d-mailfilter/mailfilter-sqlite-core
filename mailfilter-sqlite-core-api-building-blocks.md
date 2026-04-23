# mailfilter-sqlite-core API and Analysis Building Blocks

## Purpose

`mailfilter-sqlite-core` is evolving into a portable analysis core that can:

- import sequential `mailheader.log` data into SQLite
- apply `mailfilterrc` rules
- persist `decision`, `final_score`, and `rule_hits`
- reopen existing SQLite databases
- read and analyze persisted data without depending on POP3/account/transport logic

This document summarizes the currently implemented and CI-verified reader and aggregation building blocks.

---

## Current Scope

The current core already supports these major capabilities:

### Import / Analysis / Persistence

- import sequential header logs
- handle LF and CRLF input
- create and populate `messages`
- create and populate `header_entries`
- create and populate `rule_hits`
- apply score / allow / deny rules
- write `decision` and `final_score` back to `messages`

### Existing Database Reuse

- open an existing SQLite database
- validate the required schema
- read table counts from an existing database

---

## Core Database Model

The current work builds on three central relations:

- `messages`
- `header_entries`
- `rule_hits`

These are linked primarily through `msg_log_id`.

### messages
Contains the per-message view:

- `msg_log_id`
- `message_id`
- `from_addr`
- `to_addr`
- `subject`
- `date_hdr`
- `decision`
- `final_score`
- `created_at`

### header_entries
Contains parsed header rows:

- `msg_log_id`
- `ordinal`
- `tag`
- `body`

### rule_hits
Contains persisted rule-hit information:

- `msg_log_id`
- `phase`
- `expression`
- `is_negative`
- `matched`
- `header_tag`
- `header_body`
- `normalized_subject`
- `score_delta`

---

## Reader API Building Blocks

The following reader-oriented API building blocks are now available and have been exercised in the smoke tests.

### 1. Existing DB open / schema validation / counts

#### Public API
- `mf_open_existing_db(...)`
- `mf_close_existing_db()`
- `mf_validate_schema()`
- `mf_get_db_counts(...)`

#### Purpose
These functions provide the base for reopening existing analysis databases and validating that the expected schema is present before readers or aggregators are used.

---

### 2. Message readers

#### Message count
- `mf_get_message_count(int *out_count)`

#### Message summary by index
- `mf_get_message_summary_at(int index, mf_message_summary_t *out_summary)`

#### Message summary by ID
- `mf_get_message_summary_by_id(const char *msg_log_id, mf_message_summary_t *out_summary)`

#### Current `mf_message_summary_t`
The message summary structure currently includes:

- `msg_log_id`
- `decision`
- `final_score`
- `subject`
- `date_hdr`
- `created_at`
- `from_addr`
- `to_addr`

#### Notes
The `by_id` variant is especially important for building message-centric detail views, because the rest of the current read APIs already use `msg_log_id` as the natural lookup key.

---

### 3. Header readers

#### Header count for a message
- `mf_get_header_count_for_message(const char *msg_log_id, int *out_count)`

#### Header entry by index within a message
- `mf_get_header_at(const char *msg_log_id, int index, mf_header_entry_t *out_entry)`

#### Current `mf_header_entry_t`
The header entry structure currently includes:

- `msg_log_id`
- `ordinal`
- `tag`
- `body`

#### Notes
- `ordinal` is currently treated as 1-based in persisted data
- `body` has been enlarged to improve robustness for long headers such as:
  - `Received`
  - `DKIM-Signature`
  - `Authentication-Results`

The current body buffer size in the public type is intended to preserve more real-world analysis value instead of truncating important header content.

---

### 4. Rule-hit readers

#### Rule-hit count for a message
- `mf_get_rule_hit_count_for_message(const char *msg_log_id, int *out_count)`

#### Rule-hit by index within a message
- `mf_get_rule_hit_at(const char *msg_log_id, int index, mf_rule_hit_t *out_hit)`

#### Current `mf_rule_hit_t`
The rule-hit structure currently includes:

- `msg_log_id`
- `phase`
- `expression`
- `is_negative`
- `matched`
- `header_tag`
- `header_body`
- `normalized_subject`
- `score_delta`

#### Important current state
A major recent improvement is that score hits are now persisted with header context as well, so score-oriented rule hits can carry:

- `header_tag`
- `header_body`

This brings the core closer to the richer behavior already visible in real `mailfilter-sqlite` databases.

---

## Combined / Integrated Detail Use

The current reader set is already strong enough to support a message-centric detail flow:

1. read one message by `msg_log_id`
2. read its header entries
3. read its rule hits

This was explicitly exercised in the smoke test as an integrated detail check.

That means the core is no longer only a write/analysis engine, but already a practical portable read-and-explain layer.

---

## Aggregation Building Blocks

The current work has also started a first portable aggregation layer on top of `rule_hits`.

### 1. Aggregation by expression

#### Public API
- `mf_get_rule_hit_expression_agg_count(int *out_count)`
- `mf_get_rule_hit_expression_agg_at(int index, mf_rule_hit_agg_t *out_agg)`

#### Current `mf_rule_hit_agg_t`
- `expression`
- `hit_count`

#### Semantics
Version 1 currently aggregates:

- only rows with `matched = 1`
- grouped by `expression`
- ordered by:
  - `hit_count DESC`
  - `expression ASC`

#### Value
This is the first small step from individual rule-hit reading toward statistical pattern analysis.

---

### 2. Aggregation by expression + header_tag

#### Public API
- `mf_get_rule_hit_expression_tag_agg_count(int *out_count)`
- `mf_get_rule_hit_expression_tag_agg_at(int index, mf_rule_hit_tag_agg_t *out_agg)`

#### Current `mf_rule_hit_tag_agg_t`
- `expression`
- `header_tag`
- `hit_count`
- `actual_matches`
- `avg_score_impact`

#### Semantics
Version 1 currently aggregates:

- only rows with `matched = 1`
- grouped by:
  - `expression`
  - `header_tag`
- ordered by:
  - `hit_count DESC`
  - `expression ASC`
  - `header_tag ASC`

#### Value
This is much closer to the style of real SQL-based analysis already used in the main `mailfilter-sqlite` project and is especially relevant for later sensor-like analysis.

---

## Why `rule_hits` Matters

Recent work confirmed that `rule_hits` is not just a technical trace table. It already acts as:

- an explanation layer
- a sensor/marker layer
- a statistics input layer
- a later rule-generation input layer

A rule does not have to be useful only because it changes a decision strongly.
It can also be useful because it intentionally generates structured analytical data.

Examples of this design direction:

- marker-like score rules
- later possible explicit sensor rules
- later possible action-triggering rules with separate policy/evaluation layers

---

## CI-Verified Current State

The current smoke tests now verify:

### Import / analysis
- `score-lf`
- `allowdeny-lf`
- `score-crlf`

### Existing DB reopening
- open and count existing test databases

### Message readers
- count
- summary by index
- summary by `msg_log_id`

### Header readers
- count per message
- entry by index
- basic body content checks

### Rule-hit readers
- count per message
- entry by index
- contextual score-hit persistence
- allow/deny hit context checks

### Combined detail flow
- message + header + rule-hit coherence for one `msg_log_id`

### Aggregation
- aggregation by `expression`
- aggregation by `expression + header_tag`
- extended metrics:
  - `actual_matches`
  - `avg_score_impact`

---

## Design Direction for Later Work

The current implementation already suggests natural next stages.

### Possible near-term next steps
- extend aggregation further toward real SQL-style analysis
- add additional analytical metrics where useful
- document exact semantics of the existing reader and aggregation types
- build small higher-level helper functions on top of existing stable primitives

### Possible later analytical direction
- more join-based analysis between:
  - `messages`
  - `header_entries`
  - `rule_hits`
- better positive-baseline / good-mail profiling
- richer statistical analysis over header/value stability

### Possible later rule-system direction
Longer-term architecture ideas discussed during development include explicit rule classes beyond the current score/allow/deny model, such as:

- `SENSOR`
- `ACTION`

Those should remain future work, but the current reader and aggregation work already prepares the data side needed for that evolution.

---

## Summary

At the current milestone, `mailfilter-sqlite-core` is already more than a portable importer/analyzer.

It is now a portable:

- import layer
- persistence layer
- reader layer
- detail-inspection layer
- first aggregation/statistics layer

for the central `mailfilter-sqlite` database model.

That makes it a strong basis for:

- Android/mobile analysis tooling
- future admin/debug views
- later script-compatible data access
- future sensor-oriented and statistically driven analysis work
