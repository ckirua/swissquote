# C++ SBE generation contract

The Swissquote feed schema is stable and the committed generated headers are the
production wire-format implementation. Keep regeneration deterministic:

```bash
cd sbe
./generate.sh
```

`generate.sh` runs the upstream Real Logic SBE C++ generator, then applies
`patch_cpp_headers.py`. The patch adds `VarAsciiEncoding::put()` and
`VarAsciiEncoding::get()` so variable ASCII fields use the same generated API as
fixed fields and groups. Bounds and ASCII validation stay at the wire boundary;
the application codec must not add another variable-data helper.

## Schema updates

1. Edit [`bbo.xml`](bbo.xml).
2. Regenerate with `./generate.sh`.
3. Inspect the generated diff. Treat changes to block lengths, field order,
   group dimensions, or template metadata as a wire-compatibility change.
4. Update codec call sites only when the generated API or schema actually
   changes.
5. Add a boundary test for every new variable field or group.
6. Run the C++ focused tests and the full suite.
7. Commit the generated headers and this contract together.

## Generator updates

The upstream generator version is pinned in `generate.sh`. When changing it:

- regenerate into a temporary directory first;
- verify the output with `diff -ru` against the committed output after applying
  the project patch;
- verify the marker expected by `patch_cpp_headers.py` still occurs exactly
  once;
- rerun the codec tests, including malformed-frame and size-limit cases.

Do not hand-edit files below `sbe/generated/`. If the upstream header shape
changes, update the patch script and this document in the same change.
