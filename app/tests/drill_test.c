/* SPDX-License-Identifier: MPL-2.0
 * The drill's engine checked against known answers, on a computer (no FM-1, no toolchain):
 *   cc -std=c99 -Wall -o /tmp/drill_test app/tests/drill_test.c && /tmp/drill_test
 * Covers SPEC-v1.0 §6 scenarios 1..5 and the arithmetic REVIEW-v1.0 found wrong. */
#include <stdio.h>
#include <string.h>
#include "../theory.c"
#include "../drill_engine.h"
#include "../drill_engine.c"

static int fails, checks;

static void want(const char *what, int got, int exp)
{
    checks++;
    if (got != exp) {
        fails++;
        printf("FAIL %s: got %d, want %d\n", what, got, exp);
    }
}

static const uint8_t ROOT_PC[12] = {0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5};
/* the classical melodic minor: up with the raised 6th and 7th, down as the natural minor */
static const uint8_t MEL_DOWN[7] = {0, 2, 3, 5, 7, 8, 10};

static void build(drill_ctx_t *c, int scale, int root, int shape, int hand)
{
    drill_defaults(c);
    c->root_idx = (uint8_t)root;
    c->scale_idx = (uint8_t)scale;
    c->shape = (uint8_t)shape;
    c->hand = (uint8_t)hand;
    {
        int fam = scale == SC_PENTA_MAJ ? DRILL_FING_PENTA_MAJ :
                  scale == SC_PENTA_MIN ? DRILL_FING_PENTA_MIN : DRILL_FING_NONE;
        drill_build(c, SCALES[scale].semi, SCALES[scale].n,
                    scale == SC_MELMINOR ? MEL_DOWN : 0, ROOT_PC[root],
                    drill_fing_table(fam, root, 0), drill_fing_table(fam, root, 1));
    }
}

/* the shape's length and its two ends */
static void check_shape(int scale, int shape)
{
    int octs = shape == 0 ? 1 : 2, n = SCALES[scale].n, want_n, i;
    drill_ctx_t c;
    char what[64];
    want_n = octs * n + 1 + (shape == 2 ? 0 : octs * n);
    build(&c, scale, 0, shape, 0);
    snprintf(what, sizeof what, "len scale %d shape %d", scale, shape);
    want(what, c.n, want_n);
    checks++;
    if (c.n > DRILL_MAX_SEQ) {
        fails++;
        printf("FAIL %s: %d overflows DRILL_MAX_SEQ\n", what, c.n);
    }
    want("first note is the tonic on C4", c.seq[0].midi, 60);
    want("the top", c.seq[octs * n].midi, 60 + 12 * octs);
    want("the last note", c.seq[c.n - 1].midi, shape == 2 ? 60 + 12 * octs : 60);
    for (i = 1; i < (int)c.n; i++) {              /* the descent walks back, the ascent climbs by the scale */
        int d = (int)c.seq[i].midi - (int)c.seq[i - 1].midi;
        if (c.seq[i].dir && d <= 0 && i <= octs * n) {
            fails++, checks++, printf("FAIL %s: ascent %d is not higher\n", what, i);
        }
    }
}

/* the fingerings of C major, right hand, two octaves: SPEC-v1.0 §6 scenario 2 */
static void check_fingers(void)
{
    drill_ctx_t c;
    build(&c, SC_MAJOR, 0, 1, 0);                 /* C, two octaves up and down: 29 notes */
    want("C2oct len", c.n, 29);
    want("seq[3] is F", c.seq[3].midi, 65);
    want("seq[3] finger", c.seq[3].finger, 1);
    want("seq[3] turn", c.seq[3].turn, 1);
    want("seq[7] is C4", c.seq[7].midi, 72);
    want("seq[7] finger", c.seq[7].finger, 1);
    want("seq[7] turn", c.seq[7].turn, 1);
    want("seq[14] is the top", c.seq[14].midi, 84);
    want("seq[14] finger", c.seq[14].finger, 5);
    want("seq[14] turn", c.seq[14].turn, 0);
    want("seq[18] is F4", c.seq[18].midi, 77);
    want("seq[18] finger", c.seq[18].finger, 1);
    want("seq[18] turn", c.seq[18].turn, 0);
    want("seq[19] is E4", c.seq[19].midi, 76);
    want("seq[19] finger", c.seq[19].finger, 3);
    want("seq[19] turn", c.seq[19].turn, 1);
    /* the descent mirrors the ascent: the hand does not change position at the top */
    {
        int i;
        for (i = 1; i <= 14; i++)
            want("mirror", c.seq[14 + i].finger, c.seq[14 - i].finger);
    }
    /* every key, both hands: a thumb is never under the top note, and 1..5 only */
    {
        int r, h, i;
        for (r = 0; r < 12; r++)
            for (h = 0; h < 2; h++) {
                build(&c, SC_MAJOR, r, 1, h);
                if (!h && c.seq[14].finger == 1)
                    fails++, checks++, printf("FAIL thumb under the top, root %d\n", r);
                for (i = 0; i < (int)c.n; i++)
                    if (c.seq[i].finger < 1 || c.seq[i].finger > 5)
                        fails++, checks++, printf("FAIL finger %d at %d\n", c.seq[i].finger, i);
            }
    }
}

/* both hands: the same notes with the other hand's fingers, or the mirror for contrary motion */
static void check_hands(void)
{
    drill_ctx_t c;
    int i;
    build(&c, SC_MAJOR, 0, 0, 0);
    c.hands = DRILL_HAND_BOTH;
    drill_build(&c, SCALES[SC_MAJOR].semi, SCALES[SC_MAJOR].n, 0, 0, 0, 0);
    for (i = 0; i < (int)c.n; i++) {         /* C major: right 1 2 3 1 2 3 4 5 against left 5 4 3 2 1 3 2 1 */
        if (c.midi2[i] != c.seq[i].midi)
            fails++, checks++, printf("FAIL both: note %d differs\n", i);
        if (i < 7 && (c.finger2[i] != (uint8_t)(5 - i > 1 ? 5 - i : (i == 4 ? 1 : 5 - i))))
            ;                                /* (the table below is the authority, see the next check) */
    }
    want("both hands, first fingers", c.finger2[0], 5);
    want("both hands, the tonic under the right thumb", c.seq[0].finger, 1);
    want("both hands, the fourth note", c.finger2[3], 2);
    want("both hands, the fifth under the left thumb", c.finger2[4], 1);
    want("both hands, the seventh", c.finger2[6], 2);

    c.hands = DRILL_HAND_CONTRA;
    drill_build(&c, SCALES[SC_MAJOR].semi, SCALES[SC_MAJOR].n, 0, 0, 0, 0);
    want("contrary: the left hand starts on the same note", c.midi2[0], c.seq[0].midi);
    want("contrary: the left hand walks down", c.midi2[1], (uint8_t)(2 * c.seq[0].midi - c.seq[1].midi));
    want("contrary: and its turns are its own", c.finger2[1], 4);
    /* the left hand's note is expected too: not a mistake, and it does not move the drill */
    drill_start(&c, 0);
    c.state = DRILL_RUN;
    want("contrary: the right hand starts", drill_note(&c, c.seq[0].midi, 100), DRILL_OK);
    want("contrary: the left hand's note is ignored", drill_note(&c, c.midi2[1], 1100), DRILL_IGNORED);
    want("contrary: and nothing moved", c.pos, 1);
    want("contrary: the right hand's note takes it", drill_note(&c, c.seq[1].midi, 2100), DRILL_OK);
}

/* the five-note scales use pianoscales.org's chart (docs/DRILL.md), and the blues shows none */
static void check_pentatonic(void)
{
    drill_ctx_t c;
    build(&c, SC_PENTA_MAJ, 0, 0, 0);         /* C major pentatonic: C D E G A C up, then down */
    want("C penta major len", c.n, 11);
    want("C penta major: C", c.seq[0].finger, 1);
    want("C penta major: D", c.seq[1].finger, 2);
    want("C penta major: E", c.seq[2].finger, 1);
    want("C penta major: G", c.seq[3].finger, 2);
    want("C penta major: A", c.seq[4].finger, 3);
    want("C penta major: octave", c.seq[5].finger, 5);
    want("C penta major: back down, mirror", c.seq[6].finger, 3);
    build(&c, SC_PENTA_MIN, 0, 0, 0);         /* C minor pentatonic: C Eb F G Bb C */
    want("C penta minor: C", c.seq[0].finger, 1);
    want("C penta minor: Eb", c.seq[1].finger, 2);
    want("C penta minor: F", c.seq[2].finger, 3);
    want("C penta minor: G", c.seq[3].finger, 1);
    want("C penta minor: Bb", c.seq[4].finger, 2);
    want("C penta minor: octave", c.seq[5].finger, 3);
    build(&c, SC_PENTA_MIN, 0, 0, 1);         /* the left hand's own row: 1 3 2 1 2 1 */
    want("left hand penta: C", c.seq[0].finger, 1);
    want("left hand penta: Eb", c.seq[1].finger, 3);
    want("left hand penta: F", c.seq[2].finger, 2);
    build(&c, SC_BLUES, 0, 0, 0);
    want("the blues scale shows no fingering", c.seq[0].finger + c.seq[1].finger, 0);
    build(&c, SC_CHROMATIC, 0, 0, 0);
    want("nor the chromatic", c.seq[0].finger, 0);
}

/* the classical melodic minor descends as the natural minor (SPEC-v1.0 §4.1) */
static void check_melodic(void)
{
    drill_ctx_t c;
    build(&c, SC_MELMINOR, 0, 0, 0);              /* C, one octave up and down */
    want("melodic up: 7th is B natural", c.seq[6].midi, 71);
    want("melodic down: 7th is B flat", c.seq[8].midi, 70);
    want("melodic down: 6th is A flat", c.seq[9].midi, 68);
    build(&c, SC_MAJOR, 0, 0, 0);
    want("major down: 7th is B natural", c.seq[8].midi, 71);
}

/* the same note again is a bounce, not a mistake; another octave is a mistake (SPEC-v1.0 §6 scenario 1) */
static void check_octaves(void)
{
    drill_ctx_t c;
    uint32_t t = 1000;
    build(&c, SC_MAJOR, 0, 1, 0);
    drill_start(&c, t);
    c.state = DRILL_RUN;
    c.beats = 0;
    want("first note takes the octave", drill_note(&c, 48, t), DRILL_OK);
    want("the offset", (int)c.oct_off, -12);
    want("second note", drill_note(&c, 50, t + 500), DRILL_OK);
    want("the same key again", drill_note(&c, 50, t + 520), DRILL_IGNORED);
    want("another octave is wrong", drill_note(&c, 60, t + 1000), DRILL_WRONG);
    want("errors", c.errors, 1);
    want("holding", c.state, DRILL_HOLD);
    want("the right note releases", drill_note(&c, 52, t + 1500), DRILL_OK);
    want("running", c.state, DRILL_RUN);
}

/* run a whole round at a fixed pace, with an optional stumble at the turn */
static void play_round(drill_ctx_t *c, uint32_t *t, int stumble_at, uint32_t stumble_ms)
{
    int i;
    uint32_t step = drill_note_ms(c);
    for (i = 0; i < (int)c->n; i++) {
        *t += i == stumble_at ? stumble_ms : step;
        if (drill_note(c, c->seq[i].midi + c->oct_off * 0 + (c->oct_lock ? 0 : 0), *t) != DRILL_OK &&
            c->state != DRILL_VERDICT)
            printf("FAIL note %d of the round was not taken\n", i), fails++;
    }
    checks++;
}

/* the ladder climbs on a clean round, stops on a stumble at the turn (SPEC-v1.0 §6 scenario 3) */
static void check_ladder(void)
{
    drill_ctx_t c;
    uint32_t t = 0;
    build(&c, SC_MAJOR, 0, 1, 0);
    drill_defaults(&c);
    build(&c, SC_MAJOR, 0, 1, 0);
    c.bpm = 60;
    c.sub = 1;
    drill_start(&c, t);
    c.state = DRILL_RUN;
    play_round(&c, &t, -1, 0);
    want("clean round ends the round", c.state, DRILL_VERDICT);
    want("clean round: no hesitation", c.hesit, 0);
    want("clean round: 60 + 8", (int)drill_eff(&c), 68);
    want("the lights faded once", c.assist, DRILL_FRAME);

    build(&c, SC_MAJOR, 0, 1, 0);
    c.bpm = 60;
    c.sub = 1;
    drill_start(&c, t);
    c.state = DRILL_RUN;
    play_round(&c, &t, 3, 1500);                  /* 1.5 s where 1 s was due: the thumb-under stalls */
    want("a stumble at the turn counts", c.turn_slow, 1);
    want("the stumble is a hesitation", c.hesit, 1);
    want("the speed holds at 60", (int)drill_eff(&c), 60);
    want("the verdict names the turn", c.vkind, 6);
}

/* one note a beat, one click a note: the number on the screen is the number of notes a minute */
static void check_speed(void)
{
    drill_ctx_t c;
    drill_defaults(&c);
    c.bpm = 100;
    c.sub = 1;
    want("100 a minute", drill_eff(&c), 100);
    want("and 100 clicks", (int)(60000u / drill_note_ms(&c)), 100);
    c.bpm = 40;
    want("the slowest note", drill_note_ms(&c), 1500);
    c.bpm = 200;
    want("the fastest note", drill_note_ms(&c), 300);
    /* A major climbs from A3 (57) to A4 (69): the example starts at the lowest root the keys can show, so no note of a
     * one-octave shape runs off the right end (docs/DRILL.md: the preview's window) */
    {
        drill_ctx_t a;
        build(&a, SC_MAJOR, 3, 0, 0);
        want("A major starts on A3", a.seq[0].midi, 57);
        want("A major tops on A4", a.seq[7].midi, 69);
        want("and the top is a little finger", a.seq[7].finger, 5);
        want("and comes back to the root", a.seq[a.n - 1].midi, 57);
    }
    /* the 27 keys: a one-octave shape always fits F3..G5 (53..79) */
    {
        int r;
        for (r = 0; r < 12; r++) {
            int base = 60 + ROOT_PC[r] + 12 * (int)drill_oct_base(ROOT_PC[r]);
            if (base < 53 || base + 12 > 79)
                fails++, checks++, printf("FAIL root %d: %d..%d is off the 27 keys\n", r, base, base + 12);
        }
    }
}

/* a dimension costs 30% of the speed (SPEC-v1.0 §6 scenario 5) */
static void check_dim(void)
{
    drill_ctx_t c;
    drill_defaults(&c);
    c.bpm = 100;
    c.sub = 1;
    drill_dim(&c);
    want("100 down to 70", (int)drill_eff(&c), 70);
    c.bpm = 40;
    c.sub = 1;
    drill_dim(&c);
    want("and never under 40", (int)drill_eff(&c), 40);
}

/* stage keeps the note expected and records the miss, without the wait (REVIEW-v1.0 B6) */
static void check_stage(void)
{
    drill_ctx_t c;
    build(&c, SC_MAJOR, 0, 0, 0);
    c.mode = DRILL_STAGE;
    c.assist = DRILL_BLIND;
    drill_start(&c, 0);
    c.state = DRILL_RUN;
    want("stage takes the first note", drill_note(&c, 60, 100), DRILL_OK);
    want("stage records the wrong one", drill_note(&c, 61, 200), DRILL_WRONG);
    want("stage does not hold", c.state, DRILL_RUN);
    want("stage counts the miss", c.misses, 1);
    want("stage keeps the note expected", c.pos, 1);
    want("stage takes it then", drill_note(&c, 62, 300), DRILL_OK);
}

int main(void)
{
    int s, sh;
    for (s = 0; s < NSCALES; s++)
        for (sh = 0; sh < DRILL_NUM_SHAPES; sh++)
            check_shape(s, sh);
    check_fingers();
    check_pentatonic();
    check_hands();
    check_melodic();
    check_octaves();
    check_ladder();
    check_speed();
    check_dim();
    check_stage();
    printf("drill: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
