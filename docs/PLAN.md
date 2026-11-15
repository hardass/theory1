# What comes after the scales

A plan, for review. SCALE DRILL 1.9.0 does one thing: a scale, on the FM-1's 27 keys or on a piano of your own, judged
note by note, with a tempo ladder and lights that fade. This document is what could come next, in the order I would do
it, with what each step costs and what it cannot do.

**The one rule that carries over from the scales**: an exercise is offered only if its fingering exists in print. A
scale can be learnt without one; it cannot be *practised* without one. Every layer below is checked against that rule
before it is built, and the layers that fail it say so.

**The one hardware fact that shapes everything**: one MIDI channel in, no timestamps. The drill can say "you played the
wrong note" and "you stalled before the thumb-under"; it cannot say which hand played, and it cannot grade rhythm
tighter than about ±20 ms.

---

## Where the four layers sit

| # | Layer | What the player does | Status |
|---|---|---|---|
| 1 | **Scales** | a scale, up and down, in a shape | shipped (1.9.0) |
| 2 | **Arpeggios** | a triad or a seventh, broken up and down, in a shape | next |
| 3 | **Patterns** | Hanon-style figures through the scale, all keys | after 2 |
| 4 | **Progressions** | chords in a key, blocked or arpeggiated, both hands | the big one |
| 5 | **Ear** | hear a chord or an interval, play it back, in a loop | small, optional |

Each layer is another *exercise type* on the same engine: a sequence of expected notes, a judgement rule, a ladder, a
record. Nothing about the input, the lights or the store has to be reinvented - only the sequence and the rule.

## The control budget

The firmware's grammar is fixed and worth keeping: **SELECT chooses what you practise, K1 sets the root, K2..K4 the
rest, ALGO the hand, ENV where your hands are**. Four knobs is all there is, so each exercise type spends them on its
own four parameters:

| | SELECT | K1 | K2 | K3 | K4 |
|---|---|---|---|---|---|
| Scales | the scale (6) | the key | the shape | tempo | help |
| Arpeggios | the chord (see below) | the key | the shape | tempo | help |
| Patterns | the figure | the key | the shape | tempo | help |
| Progressions | the progression | the key | blocked / broken | tempo | help |

SELECT therefore becomes "which item of the current exercise type", and the exercise type itself needs one more home.
Two options:

- **A (recommended)**: the exercise is the *first* thing SELECT walks through - `SCALES -> ARPEGGIOS -> PATTERNS ->
  PROGRESSIONS`, each with its items inside. One encoder, one mental model, no new buttons.
- **B**: each exercise type gets its own row in LESSONS (a fourth topic after DRILL). More discoverable, but the topic
  list starts to look like a menu of menus.

Either way the store has to grow a dimension: today a record is `[scale][key][shape]`, and it becomes
`[exercise item][key][shape]`. With six scales and about ten arpeggio items that is **16 x 12 x 3 = 576 records**, fewer
than today's 792 - the array gets smaller, and the store version goes to 3 with a migration that maps the six kept
scales and drops the sixteen that no longer have a fingering.

---

## Layer 2 - arpeggios (the next step)

**What it is.** The notes of a chord, one at a time, up and back: `C E G C G E C`, or two octaves, or up only. It is the
natural second exercise because it uses the same hand, the same lights and the same judgement.

**Already in the firmware.** `theory.c` has the chords: `CHORDS[13]` with major, minor, diminished, augmented, sus2, sus4,
maj7, dom7, min7, half-diminished, dim7, minmaj7 and augmaj7, each with its semitones, its letters, its name in five
languages and its symbol. `degree_chord()` derives the chord on any degree of any scale (that is how the Harmony lesson
builds its chords today), and `play_chord()` already plays one broken or blocked. The sequence for an arpeggio is
`CHORDS[type].semi[]` plus the octave - five lines of code.

**Fingering - the gate.** Triad and seventh arpeggios have published fingerings, and they are *not* the scale's: one
octave of C major is `1 2 3 1 2 3 4 5`, one octave of the C major arpeggio is `1 2 3 5`. The tables are in Hanon's
appendix and in every graded syllabus, per key and per hand; they will be transcribed with the source named, exactly as
the scale tables were. **Only the types that have such a table are offered**: the major and minor triads and the maj7,
dom7 and min7 arpeggios. Diminished and augmented arpeggios are **left out** unless a printed table turns up - they are
rarely given one, and this is the same rule that removed the blues scale.

**Judgement.** Unchanged: one expected pitch class per step, the octave taken from the first note, exact pitch on the
27 keys. The lights already know the chord (`chord_mask()`), so the scale half-light becomes the chord half-light.

**Shape.** `1 octave up and down`, `2 octaves up and down`, `up only` - the same three as the scales, and the same rule
that a two-octave shape is not offered on the 27 keys.

**Tests.** The engine's own checks: length and ends of every arpeggio in every key at every shape; the fingering tables
against the source, note by note; the same "one hand, both hands, contrary" matrix as the scales; the ladder and the
store, reusing the scale tests with a different item.

**Cost.** 2-3 sessions. The risky part is not the code, it is transcribing ten fingerings per key without a typo, which
is why they go in as data with a unit test that reads them back.

---

## Layer 3 - patterns (Hanon figures)

**What it is.** A figure rather than a scale: the classic `1 2 3 4 5 4 3 2` on the notes of a scale, up through the keys,
or Hanon's No. 1 first figure. The point of these is unevenness, not pitch, and the drill already measures that.

**Already in the firmware.** Nothing specific, and nothing needed: a pattern is a list of scale degrees, so the sequence
builder takes a degree list instead of "the whole scale".

**Fingering.** Hanon's *Le Pianiste Virtuose* (1873) is public domain and prints its fingerings - the best possible
source, since the figures and the numbers come from the same book. The patterns kept, then, are exactly the ones whose
numbers are printed there.

**Judgement.** The same mono stream, plus one thing the scales already do: the turn marker, which for these figures is
every few notes and is the whole point.

**Shape.** `one octave`, `the pattern through all twelve keys`, `two octaves`.

**Cost.** 1-2 sessions. Mostly data entry and a new "degree list" path through the sequence builder.

---

## Layer 4 - progressions (the big one)

**What it is.** Chords in a key, in a musical order: `I - V - vi - IV`, `ii - V - I`, a twelve-bar blues, blocked or
broken. This is where the drill stops being a scale machine and starts being a practice partner.

**Already in the firmware.** `degree_chord()` gives the chord on every degree of every scale, with correct spelling, and
`harmony_all()` already walks a whole key's chords. The progression itself is a list of roman numerals.

**Fingering.** Here the rule bites differently: chord voicings depend on the context (inversion, what came before, the
size of the hand), so there is no published "fingering of I-V-vi-IV". **This layer shows no fingering numbers**, and that
is consistent rather than a gap: it is a chord drill, not a finger drill. The screen shows the chord symbol and the notes.

**Judgement - the new problem.** A chord is several notes at once, and the current engine expects one. The fix is small
but real: a step carries an expected **set** (a 12-bit mask, exactly what `chord_mask()` already produces), the step
completes when every note of the set has been played, and a note outside the set is an error. The wait mode, the ladder
and the lights then work unchanged. Two consequences to accept up front:

- **Timing becomes "within the step"**, not "on the beat": with no timestamps a block chord cannot be graded tighter
  than the poll.
- **The hands cannot be told apart**, so a two-hand progression is judged as one set of notes; the screen can show both
  hands' notes, the judgement cannot police them.

**Shape.** `blocked`, `broken up`, `broken up and down`.

**Store.** Progressions have no shapes in the musical sense, but they do have a tempo per progression per key - the same
array with three "shapes" reused as arrangements.

**Cost.** 3-5 sessions, most of it in the polyphonic step and in a small editor so the progression set is data, not code.

---

## Layer 5 - ear (small, optional)

**What it is.** The drill plays something and you play it back: an interval, a chord quality, a short scale fragment. The
loop and the ladder are the value; the questions exist already.

**Already in the firmware.** The quiz has ear questions (`QT_EAR_INT`, `QT_EAR_CHORD`, `QT_EAR_NOTE`) with five
languages and a scoring model. What is missing is only the loop: today a question is answered once and the quiz moves on.

**Fingering.** Not applicable - this is not a finger exercise, so under the rule it is *not* part of the drill; it would
be a "practice" mode of the quiz instead. I would keep it out of this plan unless you want it.

**Cost.** 1 session, plus a decision about where it lives.

---

## What I would not build, and why

| Idea | Why not |
|---|---|
| **A Simply Piano clone** | It listens to an acoustic piano (the FM-1 has no audio input), separates hands (one MIDI channel), shows a score (240x240 and an ASCII font), and lives on licensed arrangements. Three of those four are hardware facts, the fourth is a legal one. |
| **Score following / "play this song"** | Without timestamps it is wait-mode or nothing; without a score display it is a light show. A public-domain melody in wait mode is possible and is what a "song mode" would honestly be - but it teaches repertoire, not technique, and it is a different product. |
| **Rhythm grading** | Needs MIDI timestamps the FM-1 never sends. The IOI check we have catches a *stall*; it cannot grade a subdivision. |
| **Left/right hand policing** | One channel. Two keyboards (or a keyboard with two channels) would be a different firmware. |
| **Sight reading on the display** | The staff glyphs exist for one note at a time in the lessons; a moving two-staff score at 240x240 is not readable at arm's length, and the whole point of this device is that you look at your hands. |

## Order of work, and what it costs

| Step | Contents | Sessions |
|---|---|---|
| 0 | store v3 and the control refactor (SELECT walks exercise types, items inside) | 1-2 |
| 1 | arpeggios: sequence, fingering tables, screens, tests | 2-3 |
| 2 | patterns: degree lists, Hanon's fingerings, tests | 1-2 |
| 3 | progressions: polyphonic steps, progression set, tests | 3-5 |
| 4 | documentation, package, release | 1 |

Roughly **two weeks of evenings**, in four shippable steps - each one a version on its own, each one testable on the
unit, and none of them touching the lessons.

## Open questions for you

1. **Arpeggios: which types?** My proposal is the five with printed fingerings (major and minor triads, maj7, dom7, min7)
   and nothing else.
2. **Exercise switching**: SELECT walking `SCALES -> ARPEGGIOS -> PATTERNS -> PROGRESSIONS` (option A), or a fourth row
   in LESSONS per exercise (option B)?
3. **Progressions: which ones?** A fixed set (`I-V-vi-IV`, `ii-V-I`, `I-IV-V-I`, twelve-bar blues) or free entry by
   degree? A fixed set is a day less and covers most practice.
4. **Blocked chords**: is "all the notes of the chord within one step" acceptable, or do we need "in this order" (which
   the single-channel engine cannot really tell anyway)?
5. **Ear training**: in or out? It is the one layer that fails the fingering rule, so I have left it out of the plan.
6. **Where to stop**: scales + arpeggios is a coherent product; progressions make it a different one. Do we want that?
