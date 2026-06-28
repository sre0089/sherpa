ALTER TABLE files
ADD COLUMN parse_status TEXT NOT NULL DEFAULT 'not_parsed'
CHECK (parse_status IN ('not_parsed', 'parsed', 'parsed_with_errors', 'failed'));

CREATE TABLE symbols (
  id INTEGER PRIMARY KEY,
  file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
  kind TEXT NOT NULL CHECK (kind IN ('function', 'method', 'class', 'struct')),
  role TEXT NOT NULL CHECK (role IN ('declaration', 'definition')),
  name TEXT NOT NULL,
  qualified_name TEXT NOT NULL,
  signature TEXT NOT NULL,
  start_line INTEGER NOT NULL,
  start_column INTEGER NOT NULL,
  end_line INTEGER NOT NULL,
  end_column INTEGER NOT NULL,
  start_byte INTEGER NOT NULL,
  end_byte INTEGER NOT NULL,
  UNIQUE(file_id, kind, role, qualified_name, start_byte)
);

CREATE TABLE include_directives (
  id INTEGER PRIMARY KEY,
  file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
  target TEXT NOT NULL,
  is_system INTEGER NOT NULL CHECK (is_system IN (0, 1)),
  start_line INTEGER NOT NULL,
  start_column INTEGER NOT NULL,
  end_line INTEGER NOT NULL,
  end_column INTEGER NOT NULL,
  start_byte INTEGER NOT NULL,
  end_byte INTEGER NOT NULL,
  UNIQUE(file_id, target, start_byte)
);

CREATE TABLE diagnostics (
  id INTEGER PRIMARY KEY,
  file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
  severity TEXT NOT NULL CHECK (severity IN ('warning', 'error')),
  code TEXT NOT NULL,
  message TEXT NOT NULL,
  start_line INTEGER NOT NULL,
  start_column INTEGER NOT NULL,
  end_line INTEGER NOT NULL,
  end_column INTEGER NOT NULL,
  start_byte INTEGER NOT NULL,
  end_byte INTEGER NOT NULL
);

CREATE INDEX symbols_qualified_name_idx ON symbols(qualified_name, role);
CREATE INDEX symbols_file_kind_idx ON symbols(file_id, kind);
CREATE INDEX includes_file_idx ON include_directives(file_id);
CREATE INDEX diagnostics_file_idx ON diagnostics(file_id);

UPDATE schema_metadata SET version = 2;
