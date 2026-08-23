# Book conventions — binding for every chapter

These are contracts, not suggestions. Every chapter follows them so the book
reads as one work. If a chapter needs to break one, say so in its verification
report rather than diverging silently.

## 1. 1.13c is the unmarked default

**Patch 1.13c is the baseline.** It is the most-modded version, so it is what a
reader is assumed to be on. Write every claim as true of 1.13c, in the present
tense, with no version qualifier.

Never lead with a historical version and correct it afterwards. If the source
material was written for 1.09, the chapter is **re-centred**: 1.13c's behaviour
becomes the body, and the older behaviour becomes a marked variation.

## 2. Version notes — inline, where the reader meets them

A difference in another version is a blockquote callout placed immediately
after the claim it modifies:

```markdown
The character name occupies 16 bytes at `0014h`.

> **Version note (1.08 and earlier, save format ≤ 89):** these builds have no
> file-size or checksum field, so the name sits at `0008h` and every later
> section shifts back by `0CDh`.
```

Rules:

- Open with `**Version note (<scope>):**` in bold. The scope names game
  versions, and the format version too where one exists (save format, DT1
  version). Be specific: "1.08 and earlier", not "older versions".
- One or two sentences. Anything longer belongs in the differences table.
- State what *is* true in that version, not merely that it differs.
- Place it after the claim, never before — the reader on 1.13c should be able
  to skip every callout and still read a complete, correct chapter.
- Do not use them for mod behaviour. Mods get their own callout:
  `> **Mod note (Project Diablo 2):** …`

## 3. Differences table — one per chapter, at the end

Every chapter that has any version notes ends with a `## Version differences`
section: a compact table with 1.13c as the first data column, so a reader can
find every delta in one place without rereading.

```markdown
## Version differences

| What | 1.13c | 1.09 | 1.08 and earlier |
|---|---|---|---|
| character name offset | `0014h` | `0014h` | `0008h` |
| file size field | `0008h` | `0008h` | absent |
```

Put `—` for "not applicable" and `absent` for "the field does not exist".
A chapter with no version differences says so in one line instead.

## 4. Cite the evidence, not the confidence

Every non-obvious claim carries what settled it, inline and briefly: the
function and address (`D2Common 1.13c @ 6fd91e50`), the data checked
(`354/354 files`), or the table and its source archive. Do not write
"verified", "confirmed", or "definitely" without the evidence beside it.

Anything unverified is **marked in place** — `(unverified: …)` — or omitted.
Silence is not confirmation, and a book carries authority the claim may not
deserve.

## 5. Vanilla data only

Verify game-data claims against **vanilla archives**, never against a repo's
checked-in tables. `assets/excel/` in this repository is PD2-derived
(`objects.txt`: 626 records vs vanilla's 573), and using it silently converts
correct claims into errors.

Use `tools/d2mpq.py` to read the real archives — see
[vanilla-data.md](vanilla-data.md). Never use bare `mpyq`: D2's files are
`MPQ_FILE_IMPLODE` and mpyq returns the raw compressed sectors **without
raising**, so the read looks fine and the data is garbage (1.13c
`objects.txt` comes back as 87 records instead of 573).

**Counting rule**, so chapters agree: a record count excludes the header row
*and* the `Expansion` separator row. Vanilla sanity checks under that rule —
Objects 573, MonStats 734, MonPreset 229.

## 6. Origin and rights, for archived material

A chapter derived from someone else's work opens with an origin block naming
the author, the source page, its original date (from the page's own meta tag,
not a secondhand claim), and what was changed. Corrections credit the finding
rather than erasing it: "Siramy documented X; on 1.13c it is Y."

**Never silently modernise the author's words.** Restore what a prior
conversion dropped or altered; modern equivalents go in an editor's note beside
the original, never in place of it.

Every such chapter carries the rights line: no page in the Siramy archive
carries a licence, and republication needs the author's permission or an
explicit fair-use judgment. See [BOOK-STATUS.md](BOOK-STATUS.md).

## 7. Structure

Chapter, in order: title; origin/provenance block; narrative introduction (what
this is and why it exists); the body in stages or sections, each opening with
prose before its tables; a worked example threaded through the chapter; the
reference tables; `## Version differences`; and a link to the companion
`<name>.verification.md`.

Write for a reader who is competent but new to this format. Prose carries the
mechanism; tables carry the lookup.
