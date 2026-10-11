#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/c/journal_store.h"

int main(void) {
  uint8_t b[64];
  JournalIndex i = { 3, 5 };
  JournalIndex i2;
  JournalDays d;
  JournalDays d2;
  JournalStats st = { 1791000000u, 1791003000u, 0x2400u };
  JournalStats st2;

  assert(journal_index_encode(&i, b, 2) == 0);
  assert(journal_index_encode(&i, b, 64) == JOURNAL_INDEX_SIZE);
  assert(journal_index_decode(b, JOURNAL_INDEX_SIZE, &i2));
  assert(i2.next == 3 && i2.count == 5);
  b[1] = 7;
  assert(!journal_index_decode(b, JOURNAL_INDEX_SIZE, &i2));
  b[1] = 3;
  b[2] = 8;
  assert(!journal_index_decode(b, JOURNAL_INDEX_SIZE, &i2));

  memset(&d, 0, sizeof(d));
  d.days[2].day = 20005;
  d.days[2].distance_m = 123456;
  d.days[2].sessions = 3;
  d.days[6].day = 65535;
  assert(journal_days_encode(&d, b, 64) == JOURNAL_DAYS_SIZE);
  assert(journal_days_decode(b, JOURNAL_DAYS_SIZE, &d2));
  assert(d2.days[2].day == 20005 && d2.days[2].distance_m == 123456);
  assert(d2.days[2].sessions == 3 && d2.days[6].day == 65535);
  assert(!journal_days_decode(b, JOURNAL_DAYS_SIZE - 1, &d2));

  assert(journal_stats_encode(&st, b, 64) == JOURNAL_STATS_SIZE);
  assert(journal_stats_decode(b, JOURNAL_STATS_SIZE, &st2));
  assert(st2.first_use == st.first_use && st2.last_end == st.last_end);
  assert(st2.unlocked == 0x2400u);
  b[0] = 9;
  assert(!journal_stats_decode(b, JOURNAL_STATS_SIZE, &st2));

  printf("OK : journal_codec\n");
  return 0;
}
