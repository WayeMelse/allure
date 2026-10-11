#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/c/journal.h"

static void test_roundtrip(void) {
  JournalSession a;
  JournalSession b;
  uint8_t buf[JOURNAL_RECORD_SIZE + 10];

  memset(&a, 0, sizeof(a));
  a.activity = 1;
  a.start_time = 1791000000u;
  a.active_seconds = 3300;
  a.total_seconds = 3400;
  a.steps = 6000;
  a.distance_m = 5030;
  a.calories = 410;
  a.avg_bpm = 132;
  a.max_bpm = 171;
  a.effort_score = 31;
  a.feeling = 2;
  a.level = 3;
  a.zone_seconds[4] = 65535;
  a.achievements = (1u << ACH_WARMED_UP) | (1u << ACH_REDLINE);
  assert(journal_add_event(&a, JE_ZONE, 600, 2));
  assert(journal_add_event(&a, JE_FLAME, 2400, 1));
  assert(journal_encode(&a, buf, sizeof(buf)) == JOURNAL_RECORD_SIZE);
  assert(journal_decode(buf, JOURNAL_RECORD_SIZE, &b));
  assert(b.start_time == a.start_time && b.steps == 6000);
  assert(b.zone_seconds[4] == 65535 && b.achievements == a.achievements);
  assert(b.event_count == 2 && b.events[1].seconds == 2400);
  assert(b.events[1].type == JE_FLAME && b.events[0].value == 2);
}

static void test_limits(void) {
  JournalSession s;
  uint8_t buf[JOURNAL_RECORD_SIZE];

  memset(&s, 0, sizeof(s));
  for (int i = 0; i < 30; i++) {
    bool ok = journal_add_event(&s, JE_ZONE, (uint16_t) i, 1);
    assert(ok == (i < JOURNAL_MAX_ZONE_EVENTS));
  }
  assert(journal_add_event(&s, JE_BORNE, 5, 1));
  assert(!journal_add_event(&s, 7, 5, 1));
  assert(journal_encode(&s, buf, JOURNAL_RECORD_SIZE - 1) == 0);
  assert(journal_encode(&s, buf, JOURNAL_RECORD_SIZE) == JOURNAL_RECORD_SIZE);
  assert(!journal_decode(buf, JOURNAL_RECORD_SIZE - 1, &s));
  buf[0] = 99;
  assert(!journal_decode(buf, JOURNAL_RECORD_SIZE, &s));
}

int main(void) {
  test_roundtrip();
  test_limits();
  assert(journal_top_achievement(0) == -1);
  assert(journal_top_achievement(1u << ACH_BULLSEYE) == ACH_BULLSEYE);
  assert(journal_top_achievement((1u << ACH_WARMED_UP) |
                                 (1u << ACH_FURNACE)) == ACH_FURNACE);
  printf("OK : journal\n");
  return 0;
}
