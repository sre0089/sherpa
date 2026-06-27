PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_metadata (
  version INTEGER NOT NULL
);

INSERT INTO schema_metadata(version)
SELECT 1
WHERE NOT EXISTS (SELECT 1 FROM schema_metadata);

CREATE TABLE IF NOT EXISTS repositories (
  id INTEGER PRIMARY KEY,
  canonical_path TEXT NOT NULL UNIQUE,
  active_run_id INTEGER
);

CREATE TABLE IF NOT EXISTS index_runs (
  id INTEGER PRIMARY KEY,
  repository_id INTEGER NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
  started_at TEXT NOT NULL,
  completed_at TEXT,
  status TEXT NOT NULL CHECK (status IN ('running', 'completed', 'failed'))
);

CREATE TABLE IF NOT EXISTS files (
  id INTEGER PRIMARY KEY,
  run_id INTEGER NOT NULL REFERENCES index_runs(id) ON DELETE CASCADE,
  relative_path TEXT NOT NULL,
  language TEXT NOT NULL,
  content_fingerprint TEXT NOT NULL,
  size_bytes INTEGER NOT NULL CHECK (size_bytes >= 0),
  UNIQUE(run_id, relative_path)
);

CREATE INDEX IF NOT EXISTS files_run_path_idx ON files(run_id, relative_path);
CREATE INDEX IF NOT EXISTS index_runs_repository_idx ON index_runs(repository_id, id);
