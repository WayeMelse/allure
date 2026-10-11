#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/c/journal_store.h"

static void test_index(void) {
  JournalIndex i = { 0, 0 };

  assert(!journal_index_is_full(&i));
  assert(journal_index_slot_by_age(&i, 0) == -1);
  assert(journal_index_new_slot(&i) == 0);
  journal_index_advance(&i);
  assert(i.count == 1 && journal_index_slot_by_age(&i, 0) == 0);
  assert(journal_index_slot_by_age(&i, 1) == -1);

  for (int n = 0; n < 6; n++) {
    journal_index_advance(&i);
  }
  assert(journal_index_is_full(&i) && i.next == 0);
  assert(journal_index_slot_by_age(&i, 0) == 6);
  assert(journal_index_slot_by_age(&i, 6) == 0);

  assert(journal_index_new_slot(&i) == 0);  // ecrase la plus ancienne
  journal_index_advance(&i);
  assert(i.count == 7 && i.next == 1);
  assert(journal_index_slot_by_age(&i, 0) == 0);
  assert(journal_index_slot_by_age(&i, 6) == 1);
  assert(journal_index_slot_by_age(&i, 7) == -1);
}

static void test_days(void) {
  JournalDays d;

  memset(&d, 0, sizeof(d));
  journal_days_add(&d, 20000, 5000);
  journal_days_add(&d, 20000, 1000);
  assert(d.days[0].day == 20000 && d.days[0].sessions == 2);
  assert(d.days[0].distance_m == 6000);

  journal_days_add(&d, 20003, 2000);
  assert(d.days[1].day == 20003 && d.days[1].sessions == 1);

  journal_days_add(&d, 20010, 100);  // 20000 et 20003 hors fenetre
  assert(d.days[0].day == 20010 && d.days[0].distance_m == 100);
  assert(d.days[1].day == 20003);
}

static void test_stats(void) {
  JournalStats st = { 0, 0, 0 };
  JournalSession s;

  memset(&s, 0, sizeof(s));
  s.start_time = 1791000000u;
  s.total_seconds = 3000;
  s.achievements = (1u << ACH_WARMED_UP) | (1u << ACH_ONE_MONTH);
  journal_stats_commit(&st, &s);
  assert(st.first_use == 1791000000u && st.last_end == 1791003000u);
  assert(st.unlocked == (1u << ACH_ONE_MONTH));

  s.start_time = 1791100000u;
  s.achievements = 1u << ACH_LOYAL;
  journal_stats_commit(&st, &s);
  assert(st.first_use == 1791000000u && st.last_end == 1791103000u);
  assert(st.unlocked == ((1u << ACH_ONE_MONTH) | (1u << ACH_LOYAL)));
}

int main(void) {
  test_index();
  test_days();
  test_stats();
  printf("OK : journal_store\n");
  return 0;
}
