#include <assert.h>
#include <stdio.h>
#include "../src/miniatura_coverage.h"

static void drift_is_not_coverage(void) {
  int refs[5]; unsigned char states[5]; MiniCoverage c;
  mini_coverage_init(&c, 5, 40000, refs, states);
  mini_coverage_mark(&c, 3, MINI_COVER_PENDING);
  assert(!mini_coverage_bind(&c, 3, 0, 22397));
  assert(states[3] == MINI_COVER_FAILED && refs[3] == -1);
  assert(states[2] == MINI_COVER_UNRESOLVED); /* no rounded-PTS side effect */
  assert(mini_coverage_bind(&c, 3, 1, 32000));
  assert(refs[3] == 1 && states[3] == MINI_COVER_COVERED);
  assert(!mini_coverage_complete(&c));
  assert(!mini_coverage_bind(&c, 3, 2, 10000)); /* keep the verified frame */
  assert(refs[3] == 1 && states[3] == MINI_COVER_COVERED);
}

static void aliases_count_targets_without_duplicating_owners(void) {
  int refs[4]; unsigned char states[4]; MiniCoverage c;
  mini_coverage_init(&c, 4, 30000, refs, states);
  assert(mini_coverage_bind(&c, 0, 0, 5000));
  assert(mini_coverage_bind(&c, 1, 0, 5000));
  assert(mini_coverage_bind(&c, 2, 1, 25000));
  assert(mini_coverage_bind(&c, 3, 1, 25000));
  assert(refs[0] == refs[1] && refs[2] == refs[3]);
  assert(mini_coverage_covered_count(&c) == 4 && mini_coverage_complete(&c));
  int64_t frames[] = {5000, 25000, -1, -1};
  assert(mini_coverage_find_frame(frames, 4, 25000) == 1);
  assert(mini_coverage_find_frame(frames, 4, 5001) == -1);
  assert(mini_coverage_find_frame(frames, 4, -1) == -1);
}

static void terminal_attempts_are_not_complete(void) {
  int refs[4]; unsigned char states[4]; MiniCoverage c;
  mini_coverage_init(&c, 4, 30000, refs, states);
  assert(mini_coverage_bind(&c, 0, 2, 0));
  mini_coverage_mark(&c, 1, MINI_COVER_PENDING);
  mini_coverage_mark(&c, 2, MINI_COVER_FAILED);
  mini_coverage_mark(&c, 3, MINI_COVER_UNAVAILABLE);
  assert(mini_coverage_covered_count(&c) == 1 && !mini_coverage_complete(&c));
  mini_coverage_mark(&c, 0, MINI_COVER_PENDING); /* cannot retire a valid target */
  assert(refs[0] == 2 && states[0] == MINI_COVER_COVERED);
  assert(!mini_coverage_bind(&c, -1, 0, 0));
  assert(!mini_coverage_bind(&c, 4, 0, 0));
  assert(!mini_coverage_bind(&c, 1, 4, 10000));
}

static void endpoint_and_partial_resume(void) {
  int refs[4]; unsigned char states[4]; MiniCoverage c;
  mini_coverage_init(&c, 4, 25001, refs, states);
  assert(mini_coverage_target_ms(&c, 3) == 25001);
  assert(mini_coverage_valid(&c, 3, 25000));
  assert(!mini_coverage_valid(&c, 3, 30000)); /* no frame beyond the source */
  int64_t partial[] = {22397, 0, -1, -1};
  assert(mini_coverage_rebuild(&c, partial, 4) == 3);
  assert(refs[0] == 1 && refs[1] == -1 && refs[2] == 0 && refs[3] == 0);
  assert(states[1] == MINI_COVER_UNRESOLVED && !mini_coverage_complete(&c));
  partial[2] = 10000;
  assert(mini_coverage_rebuild(&c, partial, 4) == 4 && mini_coverage_complete(&c));
  partial[0] = -1; /* lost/corrupt saved owner cannot leave stale covered aliases */
  assert(mini_coverage_rebuild(&c, partial, 4) == 2 && !mini_coverage_complete(&c));
  assert(refs[2] == -1 && refs[3] == -1);
}

static void exact_tolerance_and_empty_inputs(void) {
  int refs[4]; unsigned char states[4]; MiniCoverage c;
  mini_coverage_init(&c, 4, 30000, refs, states);
  assert(mini_coverage_valid(&c, 2, 15000));
  assert(mini_coverage_valid(&c, 2, 25000));
  assert(!mini_coverage_valid(&c, 2, 14999));
  assert(!mini_coverage_valid(&c, 2, 25001));
  assert(!mini_coverage_valid(&c, 0, -1));
  assert(mini_coverage_rebuild(&c, NULL, 0) == 0 && !mini_coverage_complete(&c));
  mini_coverage_init(&c, 0, 30000, refs, states);
  assert(!mini_coverage_complete(&c) && mini_coverage_target_ms(&c, 0) == -1);
  mini_coverage_init(&c, 4, -1, refs, states);
  assert(!mini_coverage_valid(&c, 0, 0));
}

static void refresh_preserves_inflight_and_terminal_states(void) {
  int refs[4]; unsigned char states[4]; MiniCoverage c;
  mini_coverage_init(&c, 4, 30000, refs, states);
  mini_coverage_mark(&c, 0, MINI_COVER_PENDING);
  mini_coverage_mark(&c, 1, MINI_COVER_FAILED);
  mini_coverage_mark(&c, 2, MINI_COVER_UNAVAILABLE);
  int64_t frames[] = {-1, -1, -1, -1};
  assert(!mini_coverage_rebuild(&c, frames, 4));
  assert(states[0] == MINI_COVER_PENDING && states[1] == MINI_COVER_FAILED);
  assert(states[2] == MINI_COVER_UNAVAILABLE && states[3] == MINI_COVER_UNRESOLVED);
  frames[3] = 15000;
  assert(mini_coverage_offer_frame(&c, frames, 4, 3) == 2);
  assert(states[0] == MINI_COVER_PENDING && refs[0] == -1);
  assert(states[3] == MINI_COVER_UNRESOLVED && refs[3] == -1);
  assert(mini_coverage_rebuild(&c, frames, 4) == 2);
  assert(states[0] == MINI_COVER_PENDING && refs[0] == -1);
  assert(states[1] == MINI_COVER_COVERED && refs[1] == 3);
  assert(states[2] == MINI_COVER_COVERED && refs[2] == 3);
  assert(!mini_coverage_complete(&c));
  mini_coverage_rebuild(&c, NULL, 0);
  assert(states[0] == MINI_COVER_PENDING && states[1] == MINI_COVER_UNRESOLVED);
}

static void incremental_publication_matches_full_rebuild(void) {
  int refs[8], expectedRefs[8]; unsigned char states[8], expectedStates[8];
  MiniCoverage c, expected;
  mini_coverage_init(&c, 8, 65000, refs, states);
  mini_coverage_init(&expected, 8, 65000, expectedRefs, expectedStates);
  int64_t frames[] = {5000, 15000, 32000, 30000, 55000, 65000, 45000, 25000};
  /* Out-of-order publications include equidistant frames and closer replacements. */
  const int order[] = {6, 2, 7, 1, 4, 0, 5, 3};
  int64_t published[8];
  for (int i = 0; i < 8; i++) published[i] = -1;
  for (int i = 0; i < 8; i++) {
    int frame = order[i]; published[frame] = frames[frame];
    int covered = mini_coverage_offer_frame(&c, published, 8, frame);
    assert(covered == mini_coverage_rebuild(&expected, published, 8));
    for (int slot = 0; slot < 8; slot++) {
      assert(refs[slot] == expectedRefs[slot]);
      assert(states[slot] == expectedStates[slot]);
    }
  }
  assert(mini_coverage_complete(&c));
  assert(mini_coverage_offer_frame(&c, published, 8, 8) == 8);
}

int main(void) {
  drift_is_not_coverage();
  aliases_count_targets_without_duplicating_owners();
  terminal_attempts_are_not_complete();
  endpoint_and_partial_resume();
  exact_tolerance_and_empty_inputs();
  refresh_preserves_inflight_and_terminal_states();
  incremental_publication_matches_full_rebuild();
  puts("PASS: nominal coverage, exact PTS aliases, failure states, endpoint and partial cache rebuild");
  return 0;
}
