# Dependency pinning (Meson wraps)

This repository vendors/locks most third-party dependencies via Meson wraps in `subprojects/*.wrap`.

## Policy

- `wrap-file` dependencies are pinned by `source_url` + `source_hash` (and `patch_*` when used).
- `wrap-git` dependencies are pinned by `revision = <full commit SHA>`.
  - Do not use moving references like `master`, `main`, `head`, or release branches.

This keeps builds reproducible and reduces supply-chain risk.

## Updating a `wrap-git` dependency

1) Pick the new commit SHA in the upstream repository.
2) Edit `subprojects/<dep>.wrap` and set `revision = <sha>`.
3) Update the local checkout (optional but recommended):
   - `meson subprojects update --reset <dep>`
4) Verify:
   - `git -C subprojects/<dep> rev-parse HEAD`

## Updating a `wrap-file` dependency (WrapDB)

1) Inspect available updates:
   - `meson wrap status`
2) Update wrap file(s) from WrapDB:
   - `meson wrap update <dep>`
3) Verify the updated `source_hash`/`patch_hash` are committed.

## Notes

- `subprojects/checkasm.wrap` is a redirect used by `dav1d`; it is intentionally kept as a redirect to the pinned wrap inside that dependency.
- `subprojects/.wraplock` is an ignored build artifact and is not relied upon for locking. The authoritative pins are the committed `.wrap` files.

