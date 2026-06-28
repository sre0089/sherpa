CREATE TABLE relationships (
  id INTEGER PRIMARY KEY,
  run_id INTEGER NOT NULL REFERENCES index_runs(id) ON DELETE CASCADE,
  kind TEXT NOT NULL CHECK (kind IN ('defines', 'calls', 'includes')),
  source_file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
  source_symbol_id INTEGER REFERENCES symbols(id) ON DELETE CASCADE,
  target_file_id INTEGER REFERENCES files(id) ON DELETE CASCADE,
  target_symbol_id INTEGER REFERENCES symbols(id) ON DELETE CASCADE,
  target_text TEXT NOT NULL,
  resolution TEXT NOT NULL CHECK (resolution IN ('resolved', 'ambiguous', 'unresolved')),
  confidence TEXT NOT NULL CHECK (confidence IN ('high', 'medium', 'low')),
  provenance TEXT NOT NULL,
  candidate_count INTEGER NOT NULL CHECK (candidate_count >= 0),
  start_line INTEGER NOT NULL,
  start_column INTEGER NOT NULL,
  end_line INTEGER NOT NULL,
  end_column INTEGER NOT NULL,
  start_byte INTEGER NOT NULL,
  end_byte INTEGER NOT NULL,
  CHECK (resolution <> 'resolved' OR target_file_id IS NOT NULL OR target_symbol_id IS NOT NULL),
  CHECK (kind <> 'calls' OR source_symbol_id IS NOT NULL),
  CHECK (kind <> 'defines' OR target_symbol_id IS NOT NULL)
);

CREATE TABLE relationship_candidates (
  id INTEGER PRIMARY KEY,
  relationship_id INTEGER NOT NULL REFERENCES relationships(id) ON DELETE CASCADE,
  target_file_id INTEGER REFERENCES files(id) ON DELETE CASCADE,
  target_symbol_id INTEGER REFERENCES symbols(id) ON DELETE CASCADE,
  reason TEXT NOT NULL,
  rank INTEGER NOT NULL CHECK (rank > 0),
  CHECK (target_file_id IS NOT NULL OR target_symbol_id IS NOT NULL),
  UNIQUE(relationship_id, rank)
);

CREATE INDEX relationships_source_symbol_idx ON relationships(source_symbol_id, kind);
CREATE INDEX relationships_target_symbol_idx ON relationships(target_symbol_id, kind);
CREATE INDEX relationships_source_file_idx ON relationships(source_file_id, kind);
CREATE INDEX relationships_target_file_idx ON relationships(target_file_id, kind);
CREATE INDEX relationships_resolution_idx ON relationships(resolution, kind);
CREATE INDEX relationship_candidates_relationship_idx
ON relationship_candidates(relationship_id, rank);

UPDATE schema_metadata SET version = 3;
