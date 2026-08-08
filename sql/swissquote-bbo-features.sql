-- Idempotent feature-layer upgrade for hosts that already have swissquote.bbo.
-- Fresh installs should use swissquote-bbo.db.sql (includes full DDL).
--
-- Apply once on existing clusters:
--   clickhouse-client --multiquery < sql/swissquote-bbo-features.sql

CREATE DATABASE IF NOT EXISTS swissquote;

-- ---------------------------------------------------------------------------
-- 1) ALIAS columns on raw bbo (virtual, zero write cost)
-- ---------------------------------------------------------------------------
ALTER TABLE swissquote.bbo
    ADD COLUMN IF NOT EXISTS mid Float64 ALIAS ((bid_price + ask_price) * 0.5),
    ADD COLUMN IF NOT EXISTS spread_abs Float64 ALIAS (ask_price - bid_price),
    ADD COLUMN IF NOT EXISTS spread_bps Nullable(Float64) ALIAS if(mid = 0, NULL, spread_abs / mid * 10000),
    ADD COLUMN IF NOT EXISTS quote_age_ms Int64 ALIAS dateDiff('ms', ts, ingestion_time);

-- ---------------------------------------------------------------------------
-- 2) Plain VIEW exposing raw cols + ALIAS-derived mid/spread/quote_age
-- ---------------------------------------------------------------------------
CREATE OR REPLACE VIEW swissquote.bbo_mv AS
SELECT
    ts,
    symbol,
    platform,
    server,
    spread_profile,
    bid_price,
    ask_price,
    bid_spread,
    ask_spread,
    ingestion_time,
    mid,
    spread_abs,
    spread_bps,
    quote_age_ms
FROM swissquote.bbo;
