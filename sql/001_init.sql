CREATE TABLE IF NOT EXISTS nodes (
  id SERIAL PRIMARY KEY, 
  name TEXT UNIQUE NOT NULL
);

CREATE TABLE IF NOT EXISTS links (
  id SERIAL PRIMARY KEY,
  src INT REFERENCES nodes(id),
  dst INT REFERENCES nodes(id),
  bandwidth_mbps DOUBLE PRECISION NOT NULL,
  latency_ms DOUBLE PRECISION NOT NULL,
  cost DOUBLE PRECISION NOT NULL,
  utilization DOUBLE PRECISION DEFAULT 0,
  status TEXT NOT NULL DEFAULT 'UP'
);

CREATE TABLE IF NOT EXISTS paths (
  id SERIAL PRIMARY KEY,
  source INT REFERENCES nodes(id),
  target INT REFERENCES nodes(id),
  metric TEXT,
  constraints JSONB,
  status TEXT,   -- ACTIVE / REROUTED / UNROUTABLE
  total_cost DOUBLE PRECISION,
  created_at TIMESTAMPTZ DEFAULT now()
);

CREATE TABLE IF NOT EXISTS path_hops (
  path_id INT REFERENCES paths(id) ON DELETE CASCADE,
  seq INT,
  link_id INT REFERENCES links(id),
  PRIMARY KEY (path_id, seq)
);

CREATE INDEX IF NOT EXISTS idx_hops_link ON path_hops(link_id);

CREATE TABLE IF NOT EXISTS link_events (
  id SERIAL PRIMARY KEY,
  link_id INT NOT NULL,
  event TEXT NOT NULL,
  at TIMESTAMPTZ DEFAULT now()
);

CREATE TABLE IF NOT EXISTS path_history (
  id SERIAL PRIMARY KEY,
  path_id INT REFERENCES paths(id) ON DELETE CASCADE,
  status TEXT NOT NULL,
  total_cost DOUBLE PRECISION,
  at TIMESTAMPTZ DEFAULT now()
);
