# Architecture Milestone Note

## 1. Current Focus
`mailfilter-sqlite-core` has reached a milestone where it is no longer only an import or analysis prototype.
It now provides a **portable analysis, persistence, reopen and reader layer** for the same SQLite-centered data model used by `mailfilter-sqlite`.

Core principles in this milestone:
- **Header-only processing**
- **Deterministic rule evaluation**
- **SQLite as the central working data source**
- **Portable C/C++ reader API instead of ad-hoc JSON export**
- **Incremental, CI-verified expansion**
- **No reintroduction of POP3 / transport logic into the core**

The core remains intentionally separated from account handling, login, POP3 and transport paths.

---

## 2. Milestone Status
At this milestone, `mailfilter-sqlite-core` can:

1. **Import sequential `mailheader.log` input**
2. **Create and populate SQLite working databases**
3. **Apply real `mailfilterrc` rules**
4. **Persist `decision`, `final_score` and `rule_hits`**
5. **Reopen existing databases**
6. **Validate required schema**
7. **Read back the three central analysis tables**

This means the database is no longer a write-only target.
It is now a reusable and portable analysis source.

---

## 3. Runtime / Analysis Layer
The current core can already evaluate header data using the existing rule engine:

- `ALLOW`
- `DENY`
- `SCORE`

Verified behavior in the current milestone:
- sequential import works on **LF** and **CRLF** input
- rules from `mailfilterrc` are loaded and applied
- `decision` and `final_score` are written back into `messages`
- `rule_hits` are written for:
  - `score`
  - `allow`
  - `deny`

A key improvement in this milestone is that **score hits now also persist header context**, bringing the core closer to the richer real-world `mailfilter-sqlite` behavior.

---

## 4. Database Layer
The SQLite database remains the **central analysis component**.
The core now works on the same three-table model as the main project:

### Central tables
- `messages`
  - decision
  - final_score
  - subject
  - date_hdr
  - created_at
  - from_addr
  - to_addr
- `header_entries`
  - msg_log_id
  - ordinal
  - tag
  - body
- `rule_hits`
  - msg_log_id
  - phase
  - expression
  - matched
  - is_negative
  - header_tag
  - header_body
  - normalized_subject
  - score_delta

### Key property
These three tables are now not only populated by the core, but also **readable through stable public APIs**.

---

## 5. Reader Layer
A major milestone addition is the new **portable reader layer**.
It avoids large JSON blobs and instead uses fixed, structured C types and small reader functions.

### Current public reader coverage

#### `messages`
- count
- summary by index
- includes:
  - `msg_log_id`
  - `decision`
  - `final_score`
  - `subject`
  - `date_hdr`
  - `created_at`
  - `from_addr`
  - `to_addr`

#### `header_entries`
- count by `msg_log_id`
- entry by `msg_log_id` + index
- includes:
  - `msg_log_id`
  - `ordinal`
  - `tag`
  - `body`

#### `rule_hits`
- count by `msg_log_id`
- entry by `msg_log_id` + index
- includes:
  - `msg_log_id`
  - `phase`
  - `expression`
  - `matched`
  - `is_negative`
  - `header_tag`
  - `header_body`
  - `normalized_subject`
  - `score_delta`

### Key property
The core can now read back:
- what was decided
- which headers existed
- which rules hit
- and, increasingly, on which header context those hits occurred

This is the crucial bridge toward Android-side analysis tooling.

---

## 6. Test / Verification State
The milestone is backed by the existing CI smoke test workflow.
Current verified scenarios include:

- `score-lf`
- `allowdeny-lf`
- `score-crlf`

Verified dimensions now include:
- import success
- database creation
- schema reopen and counts
- message summaries
- header reading
- rule-hit reading
- contextual score-hit persistence
- body content checks for header reader robustness

### Key property
The milestone is not only implemented, but **continuously validated through GitHub CI**.

---

## 7. Architectural Significance
This milestone changes the position of `mailfilter-sqlite-core`.

Previously, it was mainly:
- import-oriented
- analysis-oriented
- persistence-oriented

At this stage, it is also:
- **database-reopen capable**
- **reader-capable**
- **script-bridge capable**
- **portable analysis infrastructure**

That makes it a realistic basis for partial Android-side reuse of tools such as:
- `mailfilter-analyze.py`
- `mailfilter.cgi`

---

## 8. Sensor / Action Outlook
A later architectural direction has already become visible:

Rules do not only need to act as scoring or decision rules.
They can also serve as:
- **Sensors**
- **Markers**
- **Action triggers**

Possible future rule classes:
- `SCORE`
- `ALLOW`
- `DENY`
- `SENSOR`
- `ACTION`

### Sensor concept
A rule such as `^From:` may be weak as a standalone score signal, but highly valuable as a **sensor rule** that deliberately produces `rule_hits` and statistical markers.

### Action concept
A future `ACTION` class could generate structured action events rather than directly executing side effects.
This would keep the system auditable and human-controlled.

This milestone does **not** implement that layer yet, but the richer `rule_hits` persistence now makes such a future extension much more realistic.

---

## 9. Current Milestone Summary
`mailfilter-sqlite-core` now combines:
- deterministic header analysis
- SQLite persistence
- database reopen capability
- structured readers for `messages`, `header_entries` and `rule_hits`
- contextual score-hit persistence
- CI-verified incremental growth

into a **portable analysis and reader core** that is meaningfully closer to the real analytical depth of `mailfilter-sqlite`.
