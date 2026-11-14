/* SPDX-License-Identifier: MPL-2.0
 * FM-1 SCALE DRILL: the drill's engine. Pure C, no platform, no UI: the sequence, the judging, the ladder and the
 * round's verdict. The app (app.c) draws it and the host simulator tests it (app/tests/drill_test.c).
 *
 * The spec is docs/SPEC-v1.0.md, read with docs/REVIEW-v1.0.md: the six blocking findings there (B1..B6) are why this
 * file differs from the spec's listing: no oct_shift (ui.octave is the window), no snprintf, the ladder runs on the
 * effective speed, and DRILL_STAGE keeps the note-by-note wait instead of pretending to be a rhythm test. */
#ifndef DRILL_ENGINE_H
#define DRILL_ENGINE_H

#include <stdint.h>

#define DRILL_MAX_SEQ     64            /* chromatic, 2 octaves up and down: 49 notes */
#define DRILL_NUM_SCALES  22            /* SCALES[NSCALES] in theory.c */
#define DRILL_NUM_ROOTS   12            /* the circle of fifths: C G D A E B F# Db Ab Eb Bb F */
#define DRILL_NUM_SHAPES  3             /* 0: 1 octave, up and down; 1: 2 octaves, up and down; 2: 2 octaves up */

/* the assistance: the lights fade, the screen hides more */
enum { DRILL_GUIDE, DRILL_FRAME, DRILL_ANCHOR, DRILL_BLIND, NASSIST };
/* practice (the ladder runs) or stage (blind, fixed speed); stage is kept for the engine's tests only */
enum { DRILL_PRACTICE, DRILL_STAGE };
/* where the player's hands are: a piano of their own, or the FM-1's 27 keys (one octave, and it fits) */
enum { DRILL_KB_EXT, DRILL_KB_27 };
/* the states */
enum { DRILL_IDLE, DRILL_COUNTIN, DRILL_RUN, DRILL_HOLD, DRILL_VERDICT, DRILL_SUMMARY, NSTATE };
/* what drill_note() did */
enum { DRILL_IGNORED, DRILL_OK, DRILL_WRONG, DRILL_ROUND_END };

typedef struct {
    uint8_t pc;                         /* 0..11 */
    uint8_t midi;                       /* absolute MIDI: the tonic at C4 (60), so a 1-octave shape is C4..C5 */
    uint8_t finger;                     /* 1..5, 0: no fingering shown (not a heptatonic scale) */
    uint8_t turn;                       /* 1: a thumb-under or a cross-over here */
    uint8_t dir;                        /* 1 up, 0 down */
} drill_note_t;

typedef struct {
    /* the settings */
    uint8_t root_idx, scale_idx, shape, hand;
    uint8_t assist, mode;
    uint8_t bpm, sub;                   /* notes a minute (sub is 1: one note a beat, one click a note) */
    uint8_t ioi_tol;                    /* 0 loose 45%, 1 normal 35%, 2 tight 25% */
    uint8_t exact;                      /* 1: the note itself, not its pitch class: the FM-1's own 27 keys */

    /* the sequence */
    drill_note_t seq[DRILL_MAX_SEQ];
    uint8_t n, pos;

    /* the round */
    uint8_t state;
    uint32_t t_state, t_ok, t_beat;
    uint8_t beats;                      /* the count-in's beats left */
    uint8_t prev_ok;
    int8_t oct_off;                     /* the player's octave: midi played minus the sequence's */
    uint8_t oct_lock;

    /* what the round found */
    uint8_t errors, waits, hesit, rush, misses, turn_slow;
    uint8_t last_wrong;
    uint8_t streak, round;
    uint8_t click;                      /* the app sounds a click when it sees this set, then clears it */
    uint8_t vkind;                      /* the verdict: 0 clean, 1 clean and the lights faded, 2 turn slow,
                                         * 3 errors, 4 uneven, 5 steady, 6 watch the thumb, 7 stage passed */
} drill_ctx_t;

/* the sequence from a scale's semitones: semi_down 0: the same notes down (every scale but the classical melodic
 * minor, whose descent is the natural minor's) */
void drill_build(drill_ctx_t *c, const uint8_t *semi_up, uint8_t n, const uint8_t *semi_down, uint8_t root_pc);

/* the classical melodic minor's descent (drill_build's semi_down when the scale is SC_MELMINOR) */
extern const uint8_t DRILL_MELODIC_DOWN[7];

/* the keys the lights can reach: F3..G5 */
#define DRILL_KEY_LO 53
#define DRILL_KEY_HI 79

/* the octave the sequence starts in: the lowest tonic the keys can show, so that the example climbs from a root the
 * player can see upwards (on the 27 keys it is also where the shape fits, and a one-octave shape always does) */
int8_t drill_oct_base(uint8_t root_pc);

/* what the app keeps (drill_engine.c has no storage): its own store object, so THEORY-1's settings and records are
 * never rewritten by an upgrade (docs/REVIEW-v1.0.md B1) */
typedef struct {
    uint32_t magic;
    uint8_t ver;                        /* 2: bpm is notes a minute itself, and there is a keyboard mode */
    uint8_t root_idx, scale_idx, shape, hand, assist, mode, ioi_tol, kb;
    uint8_t bpm, sub;
    uint8_t best[DRILL_NUM_SCALES][DRILL_NUM_ROOTS][DRILL_NUM_SHAPES];   /* the fastest clean round, 0: none */
    uint8_t stage[(DRILL_NUM_SCALES * DRILL_NUM_ROOTS * DRILL_NUM_SHAPES + 7) / 8];   /* a STAGE round passed */
} drill_save_t;

void drill_defaults(drill_ctx_t *c);
void drill_start(drill_ctx_t *c, uint32_t now);
int drill_note(drill_ctx_t *c, int note, uint32_t now);
void drill_tick(drill_ctx_t *c, uint32_t now);
void drill_next_round(drill_ctx_t *c, uint32_t now);
void drill_dim(drill_ctx_t *c);         /* a dimension changed (shape, hand, assistance): the speed steps back */

uint16_t drill_eff(const drill_ctx_t *c);         /* notes a minute */
uint32_t drill_note_ms(const drill_ctx_t *c);     /* the note interval, ms */
uint32_t drill_beat_ms(const drill_ctx_t *c);
uint8_t drill_allow(const drill_ctx_t *c);        /* the uneven notes a clean round allows */

#endif /* DRILL_ENGINE_H */
