# AGENTS.md

## Releasing

Bumping the version, packaging for Fab, or publishing a GitHub Release: follow `docs/RELEASING.md`. Pushing to main publishes a release whenever `VersionName` in the `.uplugin` has no matching tag yet.

## Agent skills

### Issue tracker

Issues live in GitHub Issues for `AniBullet/LanguageOne`, managed via the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` at the repo root and ADRs in `docs/adr/`. See `docs/agents/domain.md`.
