# mailfilter-sqlite-core starter

This starter package is a scaffold for an Android/NDK-friendly `mailfilter-sqlite-core`.

It is intentionally **not** a full port of the real `mailfilter-sqlite` engine yet.
Instead, it provides:

- a stable native core API (`mf_core.h`)
- a starter `CMakeLists.txt`
- a schema-aware existing SQLite open path
- a sequential header text import path
- a parser for the confirmed block format:

```text
+OK
Header: value
 continuation
.

+OK
Header: value
.
```

## Confirmed sequential block separator

The current parser accepts the block transition you confirmed:

```text
Envelope-To: xyz@domain.de

.

+OK
Return-path: <produkte@service.freenet.de>
```

Meaning:

- a single `.` on its own line ends one complete header block
- a following `+OK` begins the next block
- leading-space lines are treated as header continuations

## Current API highlights

- `mf_open_existing_db(...)`
- `mf_close_existing_db()`
- `mf_import_sequential_header_file(...)`
- `mf_parse_sequential_header_block(...)`

## Important note

This package preserves the architectural direction discussed earlier:

- keep the original source database structure unchanged
- allow only additive Android-side extensions later
- treat existing Linux-generated `.sqlite` databases as first-class inputs
- treat sequential text header logs as importable sources
