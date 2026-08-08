-- Production schema (swissquote.bbo on ClickHouse) + feature VIEW.
-- ts = quote timestamp from the API; ingestion_time = server insert time.
-- ReplacingMergeTree keeps the latest row per (symbol, platform, server, spread_profile, ts).
-- ALIAS columns on bbo are virtual (zero write cost).
-- bbo_mv is a plain VIEW over bbo (same columns including ALIAS-derived mid/spread/quote_age).

CREATE DATABASE IF NOT EXISTS swissquote;

CREATE TABLE IF NOT EXISTS swissquote.bbo
(
    ts DateTime64(3, 'UTC'),
    symbol LowCardinality(String),
    platform LowCardinality(String),
    server LowCardinality(String),
    spread_profile LowCardinality(String),
    bid_price Float64,
    ask_price Float64,
    bid_spread Float32,
    ask_spread Float32,
    ingestion_time DateTime64(3, 'UTC') DEFAULT now64(3),
    mid Float64 ALIAS ((bid_price + ask_price) * 0.5),
    spread_abs Float64 ALIAS (ask_price - bid_price),
    spread_bps Nullable(Float64) ALIAS if(mid = 0, NULL, spread_abs / mid * 10000),
    quote_age_ms Int64 ALIAS dateDiff('ms', ts, ingestion_time)
)
ENGINE = ReplacingMergeTree(ingestion_time)
PARTITION BY toYYYYMM(ts)
ORDER BY (symbol, platform, server, spread_profile, ts)
SETTINGS index_granularity = 8192;

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
