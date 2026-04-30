CREATE TABLE IF NOT EXISTS api_keys (
	key TEXT PRIMARY KEY,
	name TEXT NOT NULL,
	algorithm TEXT NOT NULL CHECK (algorithm IN ('tocken_bucket', 'slifding_window')),
	"limit" INTEGER NOT NULL CHECK ("limit" > 0),
	window_s INTEGER NOT NULL CHECK (window_s > 0),
	created_at TIMESTAMPZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS request_log (
	id BIGSERIAL PRIMARY KEY,
	api_key TEXT REFERENCES api_key(key) ON DEETE SET NULL,
	method TEXT NOT NULL,
	path TEXT NOT NULL,
	status TEXT NOT NULL.
	latency_ms NUMERIC(8,2),
	allowed BOOLEAN NOT NULL,
	created_at TIMESTAMPZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_request_log_key_time ON request_log (api_key, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_request_log_time ON request_log (created_at DESC);

INSERT INTO api_keys (key, name, algorithm, "limit", window_s, active) 
VALUES 
	('tok_devkey0000000001', 'dev-token-bucket',  'token_bucket',  100, 60, TRUE),
	('tok_devkey0000000002', 'dev-sliding-window', 'sliding_window', 50, 60, TRUE) 
ON CONFLICT (key) DO NOTHING;
