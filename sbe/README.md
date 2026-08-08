# SBE schemas

Domain SBE message schemas for the Swissquote feed. Intended to merge into **moar** in future work.

| Schema | Message | Description |
|--------|---------|-------------|
| [`bbo.xml`](bbo.xml) | `BboSnapshot` (templateId=1) | Multi-venue BBO snapshot per HTTP poll |

## Regenerate C headers

From `python/`:

```bash
make generate-sbe
```

Or directly:

```bash
../sbe/generate.sh
```

Generated C headers are committed under [`generated/`](generated/). Runtime encode/decode is in `swissquote.codec.bbo_encoder` / `swissquote.codec.bbo_decoder` (`flatten_bbo_frame` for ingest); the hand-written C++ encoder does not `#include` these headers today (schema drift reference only).
