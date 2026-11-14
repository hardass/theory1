#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# THEORY-1's tests, on a computer (no FM-1, no toolchain): the theory engine against known answers, then the whole
# firmware in fm1-base's host simulator (every quiz in every level answered, the lab, the settings,
# every page in four languages with no text off the screen).
set -e
cd "$(dirname "$0")/.."
mkdir -p app/build/test
cc -std=c99 -Wall -o app/build/test/theory_test app/tests/theory_test.c
app/build/test/theory_test
cc -std=c99 -Wall -o app/build/test/drill_test app/tests/drill_test.c
app/build/test/drill_test
python3 base/tools/build.py app --host
app/build/host/theory1_host app/tests/basic.txt app/build/test
app/build/host/theory1_host app/tests/texts_fit.txt app/build/test   # every text fits, in every language
app/build/host/theory1_host app/tests/synth.txt app/build/test        # SYNTH: every page in every language, every goal
app/build/host/theory1_host app/tests/questions.txt app/build/test   # 10800 questions made and checked
app/build/host/theory1_host app/tests/drill.txt app/build/test        # the drill: sequence, judgement, ladder, stage
echo "THEORY-1: all tests passed"
