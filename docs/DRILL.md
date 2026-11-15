# SCALE DRILL

The tenth row of **LESSONS** is not a lesson but a practice machine: pick a scale, a key, a shape and a tempo, and the
FM-1 becomes a coach. The keys light up, you play on the FM-1's own 27 keys or on a piano over USB, TRS or Bluetooth,
and every note is judged as it comes.

Everything here imitates mechanisms that the paid practice software has already proven, rather than inventing new
ones: flowkey's wait mode and loop, Melodics' auto-BPM and its lessons/stages split, Piano Marvel's accuracy thresholds,
Native Instruments' Light Guide, and the ABRSM syllabus as the external tempo reference. The sources are listed at the
end.

## Controls

The controller grammar is the one the rest of the firmware already uses: **SELECT chooses what you practise, K1 sets
the root, K2..K4 the rest**. The hint line, the two legend columns and the header spell it out.

| Control | What it does |
|---|---|
| **SELECT** | the scale (the skill): the six that have a published fingering |
| **K1** / PRESETS | the key, round the circle of fifths |
| **K2** | the shape: 1 octave up and down, 2 octaves up and down, 2 octaves up |
| **K3** | the tempo, one BPM at a time (40..200) |
| **K4** | how much the lights help: GUIDE, FRAME, ANCHOR, BLIND - and it can be turned in the middle of a round without stopping it |
| **ALGORITHM** | whose fingering is shown, in the header: `RH`, `LH`, and - on a piano of your own - `BOTH` (hands together) and `CONTR.` (contrary motion) |
| **ENV** | where the hands are: `PIANO` (your own keyboard) or `27 TASTI` (the FM-1's own keys) |
| **PLAY** | start a round (four beats of count-in, then the first note); pressed again, it stops the round and puts the screen back where it started |
| **before PLAY** | the keys are a map, not a test: play any key of the scale and the screen names the note, its finger and whether it is a turn, and the corner says which note of the shape it is |
| **HOME** | back to the topics; the record and the settings are written here |

Any change of scale, key, shape or hand plays the shape once, at 200 ms a note, so the ear knows before the hand does;
the screen names each note as it sounds, with the finger to use and the turn marked. While it plays, only the key that
sounds is lit brighter: the root and the scale stay exactly as they are, because a root that dims and brightens with
every note is unreadable (it was, until 1.7.0). PLAY interrupts it and starts the round.

### The screen

```
DRILL                          PIANO R
Sol maggiore
1 ottava                         3/15
Re  2                       <- the note to play and its finger (TURN: a thumb-under or a cross-over)
          SUONA             <- or ASPETTA (wrong note), the count-in, or the round's verdict
K1: CHIAVE Sol      K2: FORMA 1 ottava
K3: BPM 60          K4: AIUTO GUIDE
        SELECT SCALA   ENV TASTIERA
```

The header carries the keyboard mode and the hand. The two legend columns line up (K1 over K3, K2 over K4), always
showing what each knob is set to - the assistance level in words, so a turn of K4 says where it landed.

## Two keyboards

| | `PIANO` | `27 TASTI` |
|---|---|---|
| Where you play | your own piano or keyboard | the FM-1's own keys |
| Shapes | all three | one octave; asking for more switches back to `PIANO` |
| Judgement | the pitch class, at the octave you started in | the note itself |
| Octave keys | OCT- / OCT+ move what the keys sound | fixed: the sequence is placed where it fits |

The FM-1's keys are **F3..G5, MIDI 53..79**, so the lights can only ever show 26 semitones. A two-octave shape spans
25 of them, which leaves exactly three keys (F3, F#3, G3) where it fits - not worth offering. On the 27 keys the drill
therefore gives one octave, and a longer shape switches to `PIANO` rather than doing nothing.

Both modes start the sequence at **the lowest root the keys can show**: A major climbs A3 to A4, not A4 to A5, so no
note of a one-octave shape runs off the right end of the keyboard. A two-octave shape climbs from that same lowest root
and the top of it will leave the lights behind - which is the honest limit of a 27-key display, not a bug.

## One hand, both hands

| ALGORITHM | What the drill does |
|---|---|
| `RH` / `LH` | the sequence with that hand's fingering; PLAY judges it as usual |
| `BOTH` | both hands on the same notes: the screen shows `1/5` (right thumb, left little finger) and either hand's note counts, because they are the same note |
| `CONTR.` | contrary motion: the left hand walks the other way - the mirror of the shape around the tonic - and the lights show both keys. The left hand's note is expected, so it is never counted as a mistake; the right hand's note is what moves the round |

Judging is still one stream of notes: a single MIDI channel cannot say which hand played. So `BOTH` cannot tell whether
you really used both hands, and `CONTR.` judges the right hand and leaves the left alone. The screen and the lights are
the honest part of those two modes - they show the fingering and the two notes, they do not police them.

On the 27 keys there are only `RH` and `LH`: two hands on one small keyboard is not a thing to teach.

## The sequence

- The shape is built from `SCALES[]` in `theory.c`, but the drill offers only the scales that have a published keyboard
  fingering - six of them: major, natural minor, harmonic minor, melodic minor, and the two pentatonic scales. A scale
  can be learnt without a fingering; it cannot be practised without one, and this screen exists to practise. The other
  sixteen scales are still in the lessons, where they are studied rather than drilled.
- The classical **melodic minor** comes down as the natural minor (its descent has its own semitone table); every other
  scale mirrors.
- **Fingering** for the seven-note scales: one octave's table per key and hand, the top note capped with the little
  finger where the plain repeat would use the thumb, and the descent mirroring the ascent so the hand turns back
  without changing position.
- **Fingering for the pentatonic scales** (major and minor): [pianoscales.org's chart](https://www.pianoscales.org/pentatonic.html),
  all twelve keys, both hands, six values each (the five notes and the octave). There is no single accepted fingering
  for five-note scales - books differ -, but this is the one complete published chart for both hands that could be
  checked, so it is the one shown, with its source named.
- **The scales without a published fingering are not offered at all**: the blues scale, the whole tone, the octatonic,
  the chromatic and the world scales. Four sources give four different right-hand patterns for the blues scale
  (`1-3-1-3-1-3`, `1-2-3-4-1-2-3`, `4-1-2-1-2-3`, `1-2-1-2-2-3`), and the world scales would borrow the key's diatonic
  table without any book backing it. They stay in the lessons; the drill only takes what it can teach.
- **Turns** are derived, not hard-coded: a thumb under the fingers, or a finger over the thumb, is a turn. That is the
  single most common place a scale stalls, so a stumble there counts.

## Judgement

| What happens | What the drill does |
|---|---|
| right note | green flash, the next key lights, the note advances |
| PLAY pressed during a round | the round stops, the screen goes back to its starting state, and the next press counts in again |
| wrong note | the key flashes red, the round stops there (wait mode) and the answer stays lit until you play it |
| the same key again within 120 ms | ignored: a bouncing key, not a mistake |
| the right pitch class in another octave, `PIANO` | accepted: the octave is taken from your first note and held |
| the same in `27 TASTI` | a mistake: the note itself is asked for |
| gaps between notes | measured against the tempo: a note too late counts as a hesitation, too early as a rush |
| a hesitation on a turn | remembered for the round's verdict |
| eight seconds with no note | the wait ends; the note is still expected |

## The ladder, and the lights fading

A round is **clean** when it has no wrong note, no wait, no hesitation on a turn, and at most 5% of its notes uneven.

- clean: **+8 BPM**, and the lights fade one level (GUIDE -> FRAME -> ANCHOR -> BLIND)
- three wrong notes, or four uneven ones: **-8 BPM**, and one level of help comes back
- one stumble on a turn: the tempo stays where it is, and the verdict says so (`WATCH THE THUMB`)

The four levels of help:

| Level | Lights | Screen, during a round |
|---|---|---|
| **GUIDE** | the next key bright, the scale half-lit, the root in its own colour | the note's name and its finger |
| **FRAME** | the same | only the finger: the name is gone, the lights still show where |
| **ANCHOR** | the roots only, as map references | nothing |
| **BLIND** | nothing | nothing; the correct key flashes red when you miss |

The screen hides more as the lights do, which is the point: the sequence should end up in the hands, not on the
display. Before PLAY the screen always names what you press - that is how the shape is learnt in the first place.

## Records

The fastest clean round is kept per scale, key and shape, in the drill's **own store object** (`OBJ_DRILL` at
`0xDA000`, 812 bytes). THEORY-1's settings and quiz records are never rewritten by an upgrade of this fork: the app
compares the settings object's length on boot, so appending fields to it would reset them.

## Tests

| File | What it covers |
|---|---|
| `app/tests/drill_test.c` | 399 checks, plain C: every scale at every shape, the fingerings and turns of C major two octaves both ways, the mirror, all twelve keys and both hands, the classical melodic minor's descent, A major's window, the octave lock, the bounce guard, a clean round's +8, a stalled turn holding the tempo, the 30% step back for a longer or blind round, exact judgement on the 27 keys |
| `app/tests/drill.txt` | the host simulator: the way in through LESSONS, a whole round of 29 notes, a 1.5 s stall at the thumb-under, the clean round that follows, the octave lock, the 27 keys, the tempo knob, the shape's step back |
| `app/tests/texts_fit.txt` | regenerated by `gen_tests.py`: the drill drawn in five languages, four levels of help and three shapes, no text off the screen |

`sh tests/run_tests.sh` runs all of it with the rest of the firmware's tests. The new host commands are
`drill ROOT SCALE SHAPE [HAND]`, `dgo`, `dnote N`, `doff N`; the new checks are `dstate dpos dlen dbpm deff dsub derr
dhes dturn dvkind dassist dmiss dfing dkb dbest`.

## Building

The JieLi toolchain and the AC79 SDK are needed for a package (both are x86-64, so a Linux machine builds it without
CI):

```sh
sh base/tools/get_toolchain.sh                                  # ~/.jieli/toolchain
sh base/bsp/tools/get_sdk.sh                                    # ~/.jieli/ac79-sdk
cd ~/.jieli/ac79-sdk && git sparse-checkout add /apps/common/config/   # get_sdk.sh's set is short of these sources
JIELI_TOOLCHAIN=$HOME/.jieli/toolchain AC79_SDK=$HOME/.jieli/ac79-sdk ./build.sh --release 1.2.0
```

`base/` is pinned at fm1-base `b7043f9`, which accepts both flash parts (`0x856014` Puya P25Q80 and `0xB36014`
UC25HQ80): without it `flash_ok` is false on newer units and the whole store stays in RAM, so nothing is saved. That
pin is `wave00066/theory1#2`.

## Where this differs from the specification it was built from

The drill was specified in a separate document before it was written (in Chinese; the English summary is here). Six
points of it did not survive a check against the source:

1. **The store.** The spec appended a 1160-byte struct to `settings_t`; the app compares the object's length on boot,
   so that resets every quiz record once, and the struct's real size was 1592 bytes. It is a separate object instead.
2. **The lights.** `mk.hint` renders at the dimmest level, `mk.bad` at the middle one, and the host has no blinking at
   all: the four-level table could not be written as specified. The expected key uses `mk.ok`, a mistake uses `mk.bad`
   flipped by the drill every 130 ms, and the answer stays bright while the wrong key blinks.
3. **The masks.** `mk.*` are pitch-class masks, not 27-key bitmaps; the host already projects them through
   `ui.octave`. No mapping function was needed.
4. **The example.** `play_scale()` uses the lessons' fixed pace, not the drill's tempo; the drill schedules its own.
5. **The 30% step back.** The spec's test asserted it but its algorithm section did not define it.
6. **Stage mode.** "Does not stop on a wrong note" contradicted "the note stays expected"; the engine keeps the
   note-by-note wait and this fork's screen no longer offers a stage round at all.

Also: a clean round requires **no** hesitation on a turn, not merely a low total, because a stalled turn is the thing
the drill exists to catch.

## Sources

- flowkey, *Learn piano* (loop, hand, wait mode): `play.google.com/store/apps/details?id=com.flowkey.app`
- Melodics, *How it works*: `melodics.com/how-it-works`; practice mode: `support.melodics.com/en/articles/6777027`
- Melodics, the four systems that track practice: `support.melodics.com/en/articles/15091765`
- Piano Marvel, *Play mode* (80/90/96/100): `pianomarvel.com/en/feature/play-mode`
- Native Instruments, *Komplete Kontrol* Light Guide: `docs.native-instruments.com/online-guides/kontrol-s-mk3-manual/en/scale`
- ABRSM, piano syllabus 2025 & 2026 (scale speeds)
- Scale practice and errorless learning: `piano.org/theory/scale-practice/`, `truemetronome.app/blog/practice-scales-with-metronome/`,
  `bulletproofmusician.com/how-important-is-it-to-perform-a-new-skill-without-errors-from-day-1/`
- fm1-base: `github.com/charlesvestal/fm1-base`; felucca-bsp: `github.com/hugelton/felucca-bsp`
