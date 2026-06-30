ALTER TABLE index_runs
ADD COLUMN analysis_cache_version INTEGER NOT NULL DEFAULT 0;

CREATE TABLE call_sites (
  id INTEGER PRIMARY KEY,
  file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
  caller_qualified_name TEXT NOT NULL,
  caller_start_byte INTEGER NOT NULL,
  callee_text TEXT NOT NULL,
  callee_name TEXT NOT NULL,
  form TEXT NOT NULL CHECK (form IN ('unqualified', 'qualified', 'member', 'indirect')),
  argument_count INTEGER NOT NULL CHECK (argument_count >= 0),
  start_line INTEGER NOT NULL,
  start_column INTEGER NOT NULL,
  end_line INTEGER NOT NULL,
  end_column INTEGER NOT NULL,
  start_byte INTEGER NOT NULL,
  end_byte INTEGER NOT NULL,
  UNIQUE(file_id, caller_qualified_name, caller_start_byte, start_byte)
);

CREATE INDEX call_sites_file_idx ON call_sites(file_id, start_byte);

UPDATE schema_metadata SET version = 4;
