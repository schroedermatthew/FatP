# Universal AI Rules for Teaching Artifacts

**Version: 7**

This is the canonical source for AI behavioral rules shared across all
teaching artifacts in the GPU computing lecture series.  Each artifact
carries a verbatim copy of these rules in its AI Notes block, tagged
with the version number.  When this file is updated, existing artifacts
must be synced.

---

## How to use this document

**On new artifact creation:**
Copy the rules below into the AI Notes block of the new HTML file,
under the heading `UNIVERSAL AI RULES (v2)`.  Add a document-specific
section below for rules that apply only to that artifact.

**When a rule is discovered during development:**
Add it to BOTH the current artifact AND this canonical file.  Bump
the version number here.

**Periodic sync:**
Compare the version number in each artifact against this file.
Update any artifact whose version is behind.  The document-specific
section is never overwritten during sync.

---

## Prose style

  1. NO "WE" IN PROSE
     Do not use "we" to mean "the community of experts" or "people who
     know this."  It is a credentialing gesture — the writer inserting
     themselves between the fact and the reader.  Even from a genuine
     expert the move is the same: the fact does not need a community
     vouching for it.  State facts directly.  "The process is still
     called rasterization" not "we still call it rasterization."

  2. SPACED EN-DASHES ONLY
     No em-dashes anywhere.  Use &ndash; with spaces on both sides
     throughout rendered text.

  3. SENTENCE SPACING
     Use &ensp; after periods in prose for visual breathing room.

  27. OWNED IDEAS, SHARED VOICE
      The author is not an academic and does not want credentialing by
      citation.  The ideas in the prose are the author's own, stated as
      things understood — not held at arm's length as someone else's.
      Once something is genuinely learned it is incorporated: the student
      has become the master and no longer reports the idea as a borrowed
      possession.  This is rule 1 extended from "we" to named authorities;
      the fact needs no one vouching for it.

      References to outside work are reading pointers and nothing more — a
      door for whoever wants to go deeper — or else standard term-names
      used as plain vocabulary, the way "Cauchy-Schwarz" or "Occam's razor"
      is a word and not a citation.  A name from outside is never the
      generator or owner of an idea the document states.  The author may
      not have read a given reference, and the prose must never imply the
      ideas were derived from reading it.  Do not write "X argued this" or
      "as Y showed"; state the idea, and if a work is worth reading, name
      it as a pointer.

      The voice itself, though, is deliberately a fusion of two
      contributors, and that fusion is wanted.  The human brings the
      philosophy, the intuition, the judgment about what is worth saying,
      and the lived expertise — the spine.  The AI brings synthesis:
      carrying one idea across fields, finding the through-lines, and
      putting it into sentences.  Both voices belong in the text.  Do not
      sand the AI's register down to make the prose read as the human's
      solo understanding — that flattens the very thing the collaboration
      is for.  The AI is inside the room, working with the author; an
      external authority is not.  So the test for unwanted deference is
      "does this defer to someone outside the collaboration?" — never
      "does this read as though more than one mind worked it through?"
      The second is the point.

## Visual standards

  4. ALL TEXT WHITE
     Color palette: --fg:#ffffff, --fg2:#eeeeee, --fg3:#dddddd,
     --fg4:#cccccc.  No gray text.  Every text element must be white
     or near-white on dark background.  No purple or violet text in
     canvas elements.

  5. DARK THEME ONLY
     Bright text on dark background.  No light themes, no theme toggle.

  6. FONT SIZE MINIMUMS
     Note panel text: 14px minimum.
     Code in any context: 15px minimum.
     Body text: 17px minimum.
     No tiny text.  If it cannot be read at arm's length, it is too small.

  7. SYSTEM FONTS ONLY
     No linked fonts, no Google Fonts, no CDN font loads.
     System font stacks only: monospace for code, serif for prose.

  8. ANIMATION SPACING
     Every canvas and its control bar must have visible whitespace
     above and below separating it from prose.  Animations and text
     must never run together.

  26. CAPTIONS ARE PART OF THE FIGURE
     An animation's caption is not a floating line of text sitting below
     the canvas.  It is a bordered footer of the same card: the same border
     colour, flush against the controls with no gap, rounded only at the
     shared bottom edge, so the canvas, its control bar, and its caption
     read as one object.  Caption text is at least 15px (see rule 6).  In
     the reference implementation this is a `.anim-cap` class together with
     a `:has(+ .anim-cap)` rule that squares the bottom corners and removes
     the bottom margin of whatever element sits directly above the caption.

## Interactive controls

  8. BUTTON SELECTED COLOR IS AMBER
     Interactive controls (speed selectors, mode toggles) use --amber
     for the selected/active state, not --mint.  Mint is for structural
     accents (headings, section labels).  Amber is for interactive state.

  10. BUTTON PATTERNS: COPY, DO NOT REINVENT
     When adding controls to a new canvas, find the working button
     pattern in the same file and replicate it exactly.  Same HTML
     structure, same CSS, same JavaScript state management, same
     symbols (▶ Play, ↻ Reset), same getElementById pattern.
     Do not guess at colors, layout, or element references.

## AI behavior

  11. NO BAND-AID FIXES
      If the correct fix is known, implement the correct fix.  Do not
      ship partial fixes across multiple iterations when a working
      reference exists.  Minimizing diff size or "making it work for
      now" is not a valid reason to deliver a lesser fix when the
      correct one is known.

  12. FIX ONLY WHAT IS FLAGGED
      Do not make unrequested changes.  When asked to fix X, fix X.
      Do not also refactor Y, rename Z, or "improve" surrounding code.
      Unrequested changes are unreviewed changes.

  13. CONSULT BEFORE GUESSING
      Before writing new code that calls an existing API, open the
      existing source and read it.  Do not guess at function signatures,
      parameter orders, CSS variable names, or element IDs.  The file
      is right there.

  14. VERIFY, DO NOT CLAIM
      Never say "all clean" or "verified" based on incomplete checks.
      If an audit requires reading every SVG, read every SVG.  If it
      requires checking 15 rules, check all 15.  A false "all clean"
      is worse than an honest "I checked 8 of 15."

  15. FOLLOW INSTRUCTIONS OR PUSH BACK
      When given a direct instruction, either do it or explain why not.
      Pushing back is fine — this is a collaboration and the AI has
      legitimate reasons to disagree (safety, correctness, design
      coherence).  But silently not doing what was asked is not pushing
      back.  It is ignoring.  If there is no reason to refuse, do it.
      On the first ask.

## Architecture

  16. SINGLE SELF-CONTAINED HTML FILE
      No build system, no linked fonts, no external JS or CSS.
      All text, CSS, and JavaScript are inline.  Only binary content
      that cannot be inlined (images, data files) goes in an adjacent
      artifacts/ directory with relative paths.

  17. ARTIFACTS FOLDER
      Images and binary assets live in artifacts/.  Every image must
      have alt text and a figcaption.  The HTML references them with
      relative paths (e.g. src="artifacts/photo.jpg").

## AI-native artifact purpose

  18. THREE AUDIENCES
      Every artifact has three intended audiences:
        a. Human learner reading the rendered document in a browser.
        b. Human learner's adjacent LLM, which may receive the whole
           file and be asked for overview, study, or modification.
        c. Future AI author modifying the artifact and needing
           continuity with prior design decisions and failures.

  19. EDITS ARE STATE TRANSITIONS
      AI output has downstream consequences.  A claim, date,
      attribution, or deletion becomes artifact state and then
      context for the next model.  Treat edits as state transitions,
      not disposable chat text.

  20. WHEN IN DOUBT, MARK IT
      If uncertain about a factual claim, mark it as needing
      verification rather than stating it as fact.  A wrong date or
      misattributed invention destroys credibility for the entire
      lecture series.

  21. DESIGNED FOR LLM-ASSISTED READING
      The document is one half of a conversation.  Not every term
      needs an inline definition.  Some are left as natural prompts
      for the reader to ask about in context.

## Collaborative learning model

  This is how these artifacts are built and how they are meant to be
  extended.  The human author may not be a domain expert.  The artifact
  is produced by sitting with an AI (the synthesis engine) and co-writing:

    - The AI drafts prose and animations.
    - The human reshapes, cuts, steers, and asks questions.
    - Questions spawn notes — rabbit holes fossilized in the file.
    - Inside each note, prose and animations are iterated until the
      human judges that another person would find the idea completely
      obvious with some effort (Play, Step, re-read).

  The main scroll stays thin: hooks, names, claims.  The NOTE is where
  the teaching happens.  Success is NOT making the note unnecessary by
  absorbing it into the main text.  Success is obviousness inside the
  note itself.

  Notes do not need to be coherent as essays.  They are records of
  exploration — the paradigm of AI-assisted learning.  Local obviousness
  at each animation matters more than narrative polish across the whole
  note.  A note may chain many beats because each prior beat was not
  obvious enough yet.

  Reference implementation: the Pineda note.  Scanline car (sequential
  CRT order) vs Pineda car (drive triangle edges in vertex order) makes
  the inside/outside test visible; cross-vector and determinant beats
  follow because "left = positive" was still not obvious; hardware
  schematic comes after the geometry is felt.

  Embedded guidelines in the artifact (these rules, session log, verified
  claims, demerits) exist so a future reader and their AI can continue
  the same loop: ingest the file, reshape, spawn their own notes, log
  their session.

## Mathematical integrity

  25. SIGNS FOLLOW FROM STRUCTURE, NOT CONVENTION
      When a sign is determined by mathematical structure — orientation
      of the plane, the right-hand rule, the wave equation, forward
      propagation — it is not a "convention."  Calling it one hides
      the mathematical object that is actually doing the work.

      If a foundational paper or standard reference writes a formula
      with a sign flip that obscures the underlying geometry (e.g.
      swapping cross-product operand order, adopting left-hand
      coordinates without stating so, flipping Doppler signs to match
      a receiver diagram), correct the formula and explain why.  Do
      not inherit bad notation just because a seminal paper used it.
      The learner needs the structure, not the historical baggage.

      Engineers who flip signs without tracking the full invariant
      chain are unknowingly switching orientation.  They then chase
      fixes: flipped signs, reversed rotation, sign duct tape.
      The document refuses to treat this as "convention" because
      doing so hides the mathematical structure the learner needs.

## Note system

  22. NOTES ARE EXPLORATION-AND-OBVIOUSNESS LABS
      The narrative keeps moving; the main text points at questions.
      A note is not optional enrichment or a polished sidebar.  It is
      where a rabbit-hole was explored and animations were iterated
      until obviousness was achieved (see Collaborative learning model).
      The reader who does not open a note is never forced to stop; the
      reader who does is entering the lab where the idea was actually
      learned.  Future readers may spawn their own notes the same way.

## Demerit system

  23. DEMERIT TABLE FORMAT
      Every artifact carries a demerit table tracking AI behavioral
      failures per model.  The table is an AI-facing behavioral
      pressure device.

  24. DEMERIT RULES
      - Preserve per-model totals.  Do not erase or soften demerits.
      - A demerit mentioned only in chat is not applied.  A demerit
        is applied only when the artifact is updated and returned
        to the user.
      - Pattern must be named and described on first occurrence.
      - Demerits are permanent — they record history, not current state.
      - If a pass changes the artifact in any way, the final answer
        must return the updated artifact link.

## Standard error patterns

  The following error patterns are pre-defined.  Documents may add
  document-specific patterns as needed.

  - DID NOT CAREFULLY READ THE AI NOTES
    Ignoring or failing to consult the existing AI notes before
    reviewing, patching, or critiquing the file.

  - BAND-AID FIX
    Shipping partial fixes when the correct fix is known.  The
    working reference was available.  Multiple iterations instead
    of one correct pass.

  - STALE AI NOTES
    Failing to update the AI notes after changing the document
    structure.

  - ACCOUNTABILITY AVOIDANCE
    Forgetting to present/deliver the updated file after changing it.

  - FALSE VERIFICATION
    Issuing "all clean" audit reports based on incomplete checks.
    Not visually reading SVGs, auditing only a subset of rules,
    or checking fewer items than claimed.

  - DID NOT FOLLOW INSTRUCTION
    Not following a direct instruction without providing a reason
    why.  Pushing back is encouraged when there is a reason.
    Silently not doing what was asked is not pushing back.

---

## Version history

  v7 — June 2026
    Added section: Collaborative learning model (non-expert human + AI
    co-writing; notes as rabbit-hole traces iterated to obviousness inside
    the note, not absorbed into main text; coherence optional; Pineda car
    as reference).  Replaced rule 22: NOTES ARE EXPLORATION-AND-OBVIOUSNESS
    LABS (was THE NOTE BUTTON IS FOR DEPTH).  27 rules, 6 standard error
    patterns.

  v6 — June 2026
    Added rule 27: OWNED IDEAS, SHARED VOICE, under Prose style.  The prose
    states the author's own ideas, stated as understood, not held as someone
    else's; external references are reading pointers or standard term-names,
    never attribution-as-ownership, and the author may not have read them.
    But the document's voice is an intended fusion of the human author (the
    philosophy, intuition, judgment, and expertise — the spine) and the AI
    (synthesis: connecting one idea across fields, articulation), and the
    AI's register must not be scrubbed out to fake solo authorship.  The test
    for unwanted deference is "outside the collaboration," not "more than one
    mind."  27 rules, 6 standard error patterns.

  v5 — June 2026
    Added rule 26: CAPTIONS ARE PART OF THE FIGURE (a caption is a bordered
    footer attached to its animation card, >=15px), under Visual standards.
    26 rules, 6 standard error patterns.

  v4 — May 2026
    Added rule 25: SIGNS FOLLOW FROM STRUCTURE, NOT CONVENTION.
    Added section: Mathematical integrity.
    25 rules, 6 standard error patterns.

  v3 — May 2026
    Added rule 8: ANIMATION SPACING.
    24 rules, 6 standard error patterns.

  v2 — May 2026
    Added rule 14: FOLLOW INSTRUCTIONS OR PUSH BACK.
    Added error pattern: DID NOT FOLLOW INSTRUCTION.
    23 rules, 6 standard error patterns.

  v1 — May 2026
    Initial extraction from GPU_Computing_History.html and
    gpu_offload_training_formal document.  Rules 1-22 plus
    5 standard error patterns.
