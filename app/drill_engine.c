/* SPDX-License-Identifier: MPL-2.0
 * FM-1 SCALE DRILL: the engine's body. See drill_engine.h for what it is and docs/REVIEW-v1.0.md for why it is not
 * the spec's listing verbatim (no snprintf here: the verdict is a code the app turns into five languages). */
#include "drill_engine.h"

/* the fingering of one octave, thumb 1 .. little 5, by the circle of fifths (C G D A E B F# Db Ab Eb Bb F). The thumb
 * never lands on a black key; a key whose one octave is not the plain 1 2 3 1 2 3 4 starts on another finger.
 * docs/REVIEW-v1.0.md I1: these are the tables the spec gives, kept as data so a teacher corrects them without code */
static const uint8_t FINGER_R[DRILL_NUM_ROOTS][7] = {
    {1, 2, 3, 1, 2, 3, 4}, {1, 2, 3, 1, 2, 3, 4}, {1, 2, 3, 1, 2, 3, 4}, {1, 2, 3, 1, 2, 3, 4},
    {1, 2, 3, 1, 2, 3, 4}, {1, 2, 3, 1, 2, 3, 4}, {2, 3, 4, 1, 2, 3, 1}, {2, 3, 1, 2, 3, 4, 1},
    {3, 4, 1, 2, 3, 1, 2}, {3, 1, 2, 3, 4, 1, 2}, {4, 1, 2, 3, 1, 2, 3}, {1, 2, 3, 4, 1, 2, 3},
};
static const uint8_t FINGER_L[DRILL_NUM_ROOTS][7] = {
    {5, 4, 3, 2, 1, 3, 2}, {5, 4, 3, 2, 1, 3, 2}, {5, 4, 3, 2, 1, 3, 2}, {5, 4, 3, 2, 1, 3, 2},
    {5, 4, 3, 2, 1, 3, 2}, {4, 3, 2, 1, 4, 3, 2}, {4, 3, 2, 1, 3, 2, 1}, {3, 2, 1, 4, 3, 2, 1},
    {3, 2, 1, 4, 3, 2, 1}, {3, 2, 1, 4, 3, 2, 1}, {3, 2, 1, 4, 3, 2, 1}, {5, 4, 3, 2, 1, 3, 2},
};
/* the top note's finger where the table's plain repeat reads wrong to a teacher: 0 keeps the repeat's finger
 * (a thumb is lifted to the little finger, see drill_build) */
static const uint8_t TOP_R[DRILL_NUM_ROOTS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t TOP_L[DRILL_NUM_ROOTS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#define DRILL_BPM_MIN 40
#define DRILL_EFF_MAX 480               /* 120 a beat, four notes a beat: the fastest the ladder reaches */

/* the classical melodic minor comes down as the natural minor does */
const uint8_t DRILL_MELODIC_DOWN[7] = {0, 2, 3, 5, 7, 8, 10};

static uint8_t clamp_bpm(int v)
{
    return (uint8_t)(v < DRILL_BPM_MIN ? DRILL_BPM_MIN : v > 200 ? 200 : v);
}

uint16_t drill_eff(const drill_ctx_t *c) { return (uint16_t)c->bpm * c->sub; }

uint32_t drill_beat_ms(const drill_ctx_t *c) { return 60000u / (c->bpm ? c->bpm : 60u); }

uint32_t drill_note_ms(const drill_ctx_t *c)
{
    uint32_t beat = drill_beat_ms(c);
    return beat / (c->sub ? c->sub : 1u);
}

uint8_t drill_allow(const drill_ctx_t *c)
{
    int a = ((int)c->n * 5 + 99) / 100;          /* 5% of the round, at least one (docs/REVIEW-v1.0.md S3) */
    return (uint8_t)(a < 1 ? 1 : a);
}

void drill_defaults(drill_ctx_t *c)
{
    int i;
    uint8_t *p = (uint8_t *)c;
    for (i = 0; i < (int)sizeof *c; i++)
        p[i] = 0;
    c->shape = 0;
    c->hand = 0;
    c->assist = DRILL_GUIDE;
    c->mode = DRILL_PRACTICE;
    c->sub = 1;
    c->bpm = 60;
    c->ioi_tol = 1;
    c->state = DRILL_IDLE;
}

/* one note of the sequence: added if it fits */
static int push(drill_ctx_t *c, int midi, int dir)
{
    drill_note_t *e;
    if (c->n >= DRILL_MAX_SEQ || midi < 0 || midi > 127)
        return 0;
    e = &c->seq[c->n++];
    e->midi = (uint8_t)midi;
    e->pc = (uint8_t)(midi % 12);
    e->finger = 0;
    e->turn = 0;
    e->dir = (uint8_t)dir;
    return 1;
}

void drill_build(drill_ctx_t *c, const uint8_t *semi_up, uint8_t n, const uint8_t *semi_down, uint8_t root_pc)
{
    int octs = c->shape == 0 ? 1 : 2, i, oct, top;
    int base = 60 + (int)root_pc + 12 * (int)drill_oct_base(root_pc);
    const uint8_t *tab = c->hand ? FINGER_L[c->root_idx % DRILL_NUM_ROOTS] : FINGER_R[c->root_idx % DRILL_NUM_ROOTS];
    uint8_t topfix = c->hand ? TOP_L[c->root_idx % DRILL_NUM_ROOTS] : TOP_R[c->root_idx % DRILL_NUM_ROOTS];
    c->n = 0;
    c->pos = 0;
    if (!semi_up || n == 0 || n > 12) {
        c->n = 0;
        return;
    }
    for (oct = 0; oct < octs; oct++)
        for (i = 0; i < (int)n; i++)
            push(c, base + 12 * oct + semi_up[i], 1);
    push(c, base + 12 * octs, 1);                /* the top: the tonic over the last octave */
    if (c->shape != 2)                           /* down, to the tonic: the descent's own notes if it has them */
        for (oct = octs - 1; oct >= 0; oct--)
            for (i = (int)n - 1; i >= 0; i--)
                push(c, base + 12 * oct + (semi_down ? semi_down[i] : semi_up[i]), 0);

    /* the fingering: the plain repeat up, a little finger on a top the repeat would take with the thumb, then the
     * descent mirrors the ascent so the hand turns back without changing position. A scale of other than seven notes
     * shows none: a wrong fingering teaches worse than no fingering (docs/REVIEW-v1.0.md I1) */
    if (n != 7 || c->n < 2) {
        for (i = 0; i < (int)c->n; i++)
            c->seq[i].finger = 0;
    } else {
        for (i = 0; i < (int)c->n; i++)
            if (c->seq[i].dir)
                c->seq[i].finger = tab[i % 7];
        top = octs * (int)n;
        if (topfix)
            c->seq[top].finger = topfix;
        else if (!c->hand && c->seq[top].finger == 1)
            c->seq[top].finger = 5;              /* a thumb under the top note: the little finger caps it */
        for (i = top + 1; i < (int)c->n; i++) {
            int m = 2 * top - i;
            c->seq[i].finger = m >= 0 ? c->seq[m].finger : 0;
        }
    }
    /* both hands: the other hand's fingers at every step, and its notes (the same ones, or the mirror of the tonic
     * for contrary motion, where the left hand walks down as the right walks up) */
    if (c->hands >= DRILL_HAND_BOTH) {
        drill_ctx_t t;
        drill_defaults(&t);
        t.root_idx = c->root_idx;
        t.shape = c->shape;
        t.hand = (uint8_t)!c->hand;
        drill_build(&t, semi_up, n, semi_down, root_pc);
        for (i = 0; i < (int)c->n; i++) {
            c->finger2[i] = t.seq[i].finger;
            c->midi2[i] = (uint8_t)(c->hands == DRILL_HAND_CONTRA ? 2 * base - (int)c->seq[i].midi : c->seq[i].midi);
        }
    }
    /* the turns: a thumb under (the note before took three or more), or a finger over a thumb */
    c->seq[0].turn = 0;
    for (i = 1; i < (int)c->n; i++) {
        drill_note_t *p = &c->seq[i - 1], *e = &c->seq[i];
        e->turn = (uint8_t)((e->finger == 1 && p->finger >= 3) || (p->finger == 1 && e->finger >= 3));
    }
}

/* the lowest tonic the keys can show (F3, MIDI 53): the example starts there and climbs, so as much of it as possible
 * lands on keys that light */
int8_t drill_oct_base(uint8_t root_pc)
{
    int base = 60 + (int)root_pc, oct = 0;
    while (base - 12 >= DRILL_KEY_LO)
        base -= 12, oct--;
    while (base < DRILL_KEY_LO)
        base += 12, oct++;
    return (int8_t)oct;
}

void drill_start(drill_ctx_t *c, uint32_t now)
{
    if (!c->n)
        return;
    c->pos = 0;
    c->errors = c->waits = c->hesit = c->rush = c->misses = c->turn_slow = 0;
    c->last_wrong = 255;
    c->oct_lock = 0;
    c->oct_off = 0;
    c->prev_ok = 255;
    c->t_ok = now;
    c->t_beat = now;
    c->beats = 4;
    c->click = 0;
    c->vkind = 0;
    c->state = DRILL_COUNTIN;
    c->t_state = now;
}

void drill_dim(drill_ctx_t *c)
{
    c->bpm = clamp_bpm((int)c->bpm * 7 / 10);
}

void drill_next_round(drill_ctx_t *c, uint32_t now)
{
    c->round++;
    drill_start(c, now);
}

/* the round's verdict: which words, and (for the ladder) what the speed became */
static void finish(drill_ctx_t *c, uint32_t now)
{
    int uneven = c->hesit + c->rush;
    /* a clean round: no wrong note, no wait, no hesitation on a turn (that is the whole point of the drill), and at
     * most 5% of the notes uneven. docs/REVIEW-v1.0.md §S3 against SPEC §6.3: one stumble on the thumb-under must stop
     * the ladder even when the round's total is small */
    uint8_t clean = (uint8_t)(c->errors == 0 && c->waits == 0 && !c->turn_slow && uneven <= drill_allow(c));
    c->state = DRILL_VERDICT;
    c->t_state = now;
    if (c->mode == DRILL_STAGE) {
        if (clean) {
            c->vkind = 7;                        /* STAGE passed: the app keeps the record */
        } else {
            c->vkind = 3;
        }
        return;
    }
    if (clean) {
        if (c->assist < DRILL_BLIND) {
            c->assist++;                         /* the lights fade by themselves: no relying on the dots */
            c->streak++;
            c->vkind = 1;
        } else {
            c->vkind = 0;
        }
        c->bpm = clamp_bpm((int)c->bpm + 8);
        return;
    }
    if (c->errors >= 3 || uneven >= 4) {
        c->bpm = clamp_bpm((int)c->bpm - 8);
        if (c->turn_slow)
            c->vkind = 2;
        else if (c->errors >= 3)
            c->vkind = 3;
        else
            c->vkind = 4;
        if (c->assist > DRILL_GUIDE && c->errors >= 3)
            c->assist--;                         /* three wrong: one level of help back */
        return;
    }
    c->vkind = c->turn_slow ? 6 : 5;
}

static int ok(drill_ctx_t *c, int note, uint32_t now)
{
    int was_hold = c->state == DRILL_HOLD;
    if (!was_hold && c->pos > 0 && c->oct_lock) {
        uint32_t target = drill_note_ms(c), dt = now - c->t_ok;
        int tol = c->ioi_tol == 0 ? 45 : c->ioi_tol == 2 ? 25 : 35;
        if (target) {
            if ((int32_t)dt > (int32_t)(target + target * (uint32_t)tol / 100u)) {
                c->hesit++;
                if (c->seq[c->pos].turn)
                    c->turn_slow = 1;
            } else if ((int32_t)dt < (int32_t)(target - target * (uint32_t)tol / 100u)) {
                c->rush++;
            }
        }
    }
    c->t_ok = now;
    c->prev_ok = (uint8_t)note;
    c->state = DRILL_RUN;
    c->last_wrong = 255;
    c->pos++;
    if (c->pos >= c->n) {
        finish(c, now);
        return DRILL_ROUND_END;
    }
    return DRILL_OK;
}

static int wrong(drill_ctx_t *c, int note, uint32_t now)
{
    c->errors++;
    c->last_wrong = (uint8_t)note;
    if (c->mode == DRILL_STAGE) {
        c->misses++;                             /* stage: the note stays expected, the round goes on */
        return DRILL_WRONG;
    }
    c->waits++;
    c->state = DRILL_HOLD;
    c->t_state = now;
    return DRILL_WRONG;
}

int drill_note(drill_ctx_t *c, int note, uint32_t now)
{
    const drill_note_t *e;
    int target;
    if (note < 0 || note > 127 || !c->n)
        return DRILL_IGNORED;
    if (c->state != DRILL_RUN && c->state != DRILL_HOLD)
        return DRILL_IGNORED;
    if (c->pos >= c->n)
        return DRILL_IGNORED;
    if (c->pos > 0 && c->prev_ok != 255 && (uint8_t)note == c->prev_ok && (uint32_t)(now - c->t_ok) < 120u)
        return DRILL_IGNORED;                    /* the same key struck again: a bounced key, not a mistake */
    e = &c->seq[c->pos];
    /* with both hands the other hand's note is expected too: it is not a mistake, and it does not move the drill */
    if (c->hands >= DRILL_HAND_BOTH && c->pos < c->n && c->midi2[c->pos] != e->midi &&
        (uint8_t)(note % 12) == (uint8_t)(c->midi2[c->pos] % 12))
        return DRILL_IGNORED;
    if (c->exact)
        return note == (int)e->midi ? ok(c, note, now) : wrong(c, note, now);
    if (!c->oct_lock) {
        if ((uint8_t)(note % 12) != e->pc)
            return wrong(c, note, now);
        c->oct_off = (int8_t)(note - (int)e->midi);   /* where the player's keyboard sits, taken from the first note */
        c->oct_lock = 1;
        return ok(c, note, now);
    }
    target = (int)e->midi + (int)c->oct_off;
    if (note != target)
        return wrong(c, note, now);
    return ok(c, note, now);
}

void drill_tick(drill_ctx_t *c, uint32_t now)
{
    uint32_t beat = drill_beat_ms(c);
    if (c->state == DRILL_IDLE || c->state == DRILL_SUMMARY)
        return;
    if ((int32_t)(now - c->t_beat) >= (int32_t)beat) {
        c->t_beat += beat;
        c->click = 1;                            /* the app sounds it and clears it: a short high note */
        if (c->state == DRILL_COUNTIN && c->beats && !--c->beats) {
            c->state = DRILL_RUN;
            c->t_state = now;
            c->t_ok = now;
        }
    }
    if (c->state == DRILL_HOLD && (int32_t)(now - c->t_state) > 8000)
        c->state = DRILL_RUN;                    /* eight seconds stuck: the wait ends, the note stays expected */
    if (c->state == DRILL_VERDICT && (int32_t)(now - c->t_state) >= 1500)
        drill_next_round(c, now);
}
