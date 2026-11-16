# NoirGFX: the owner's answers

Every answer the owner gave while I0079 was shaped, verbatim, as the record the issue points to
(session `1d6b0065-98f8-4f31-a625-0352185ff311`, first recorded 2026-11-16T05:09:41Z, all on 2026-11-16).

## 1. Noir graphics pack for OpenTTD (derived from OpenGFX): which sprite fidelity should the first version target?

> 8bpp 1x first (Recommended)

## 2. How should AI image generation be used in the noir pack?

> Hero + img2img by category

## 3. Where should the first deliverable stop so you can review it in-game?

> Vertical slice (Recommended)

## 4. (follow-up message, supersedes the fidelity answer)

> rad bych pomoci ai/llm udelal celej pack / grafiku v tomhle ostrym stylu, nejak ciste to dostal do toho projektu, predelal veskere tiles, nejak automatizovane, ciste, plugable, at je to takhle dark noinr, do it for me, vymsli to, vse udelej dokonale, hezky, mam dokonce i licence, musi to byt hodne sylizovane, vsechny tiles high res v tomto stylu, bezesve, navazujici nainstaluj co treba jakkykoliv tooling required

## 5. (follow-up message, 2026-11-16)

> /Users/korczis/Downloads/Kenney\ Game\ Assets\ 3\ version\ 28.zip /Users/korczis/Downloads/Kenney\ Game\ Assets\ 2\ version\ 22.zip tohle rozbal a uloz do repo bude tim nahrazovat vetsinu puvodnich assets a stylizovat do korczis noir

## 6. The Kenney bundles (CC0) are extracted into tools/research/noirgfx/assets/kenney/ in the public korczis/OpenTTD fork: 658 MB, 38,486 files. Once pushed to GitHub, binaries stay in history forever. How should they be stored in git?

> Curated subset via LFS (Recommended)

## 7. (follow-up message, 2026-11-16)

> ano mas ma veskera svoleni

## 8. GitHub refused the push: Git LFS objects cannot be uploaded to a public fork (korczis/OpenTTD is a fork of OpenTTD/OpenTTD). Nothing was pushed. The curated Kenney models are 16 MB, 753 files, the largest 358 KB. How should they be stored? (2026-11-16, session 1d6b0065-98f8-4f31-a625-0352185ff311)

Options offered: "Normálně v gitu (Recommended)", "Necommitovat modely", "Počkat na odpojení forku".

> Necommitovat modely

This replaces answer 6: the models stay local, produced by `noirgfx curate` from the two bundles
and verified against `tools/research/noirgfx/assets/kenney-curated.sha256`.


## 9. (follow-up message, 2026-11-16, session 1d6b0065-98f8-4f31-a625-0352185ff311)

> zamitni  the-game-wears-the-owners-style

Recorded as `cancelled: true` on `.ai/repo/project/intents/the-game-wears-the-owners-style.yaml`
(the intent schema stores no status; cancellation is the fact it records). NoirGFX itself stays.

## 10. (follow-up message, 2026-11-16, same session)

> by defatult musi byt defafaultni grafika

Already so: `noirgfx.obg` carries `fallback = 1`, so OpenTTD's automatic choice is OpenGFX, and the
owner's `openttd.cfg` names OpenGFX.
