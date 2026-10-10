# SBE schemas

Domain SBE message schemas for the Swissquote feed. Intended to merge into **moar** in future work.

| Schema | Message | Description |
|--------|---------|-------------|
| [`bbo.xml`](bbo.xml) | `BboSnapshot` (templateId=1) | Multi-venue BBO snapshot per HTTP poll |

## Regenerate headers

Requires Java 17+ and the SBE generator jar:

```bash
cd sbe
SBE_TARGET_LANGUAGE=Cpp ./generate.sh
```

Use `SBE_OUT=/tmp/sbe-cpp` to inspect generated headers before replacing the
committed output.

Generated C++ headers are committed under [`generated/`](generated/) and are
used directly by the production C++ encoder/decoder. The generated classes own
the SBE message header, fixed fields, repeating groups, and variable ASCII
payload access. The small project-owned extension is applied reproducibly by
[`patch_cpp_headers.py`](patch_cpp_headers.py); it is not an application-level
codec helper.

See [`CPP_GENERATOR.md`](CPP_GENERATOR.md) for the schema and generator update
contract.
