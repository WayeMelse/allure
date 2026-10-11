#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/c/achievements.h"

#define HAS(m, a) (((m) >> (a)) & 1)
#define T0 1791000000u
#define DAY 86400u

static JournalSession base(void) {
  JournalSession s;
  memset(&s, 0, sizeof(s));
  s.start_time = T0;
  s.active_seconds = 300;
  return s;
}

static uint16_t run(const JournalSession *s, const JournalStats *st,
                    const JournalDays *d, int hour) {
  JournalGoals g = { 0, 1500, 50000, 5 };
  return achievements_compute(s, st, d, &g, hour, 20000);
}

static void test_simple(void) {
  JournalSession s = base();
  JournalStats st;
  JournalDays d;
  memset(&st, 0, sizeof(st));
  memset(&d, 0, sizeof(d));
  assert(run(&s, &st, &d, 12) == 0);
  s.active_seconds = 601;
  assert(HAS(run(&s, &st, &d, 12), ACH_WARMED_UP));
  s.active_seconds = 600;
  assert(!HAS(run(&s, &st, &d, 12), ACH_WARMED_UP));
  assert(HAS(run(&s, &st, &d, 6), ACH_EARLY_BIRD));
  assert(!HAS(run(&s, &st, &d, 7), ACH_EARLY_BIRD));
  assert(HAS(run(&s, &st, &d, 20), ACH_NIGHT_OWL));
  assert(!HAS(run(&s, &st, &d, 19), ACH_NIGHT_OWL));
}

static void test_session(void) {
  JournalSession s = base();
  JournalStats st;
  JournalDays d;
  JournalGoals steps = { 1, 6000, 50000, 5 };
  memset(&st, 0, sizeof(st));
  memset(&d, 0, sizeof(d));
  s.calories = 499;
  assert(!HAS(run(&s, &st, &d, 12), ACH_FIRE_STARTER));
  s.calories = 500;
  assert(HAS(run(&s, &st, &d, 12), ACH_FIRE_STARTER));
  assert(!HAS(run(&s, &st, &d, 12), ACH_FURNACE));
  s.calories = 1000;
  assert(HAS(run(&s, &st, &d, 12), ACH_FURNACE));
  assert(!HAS(run(&s, &st, &d, 12), ACH_FIRE_STARTER));
  s.redline_seconds = 300;
  assert(!HAS(run(&s, &st, &d, 12), ACH_REDLINE));
  s.redline_seconds = 301;
  assert(HAS(run(&s, &st, &d, 12), ACH_REDLINE));
  s.distance_m = 1499;
  assert(!HAS(run(&s, &st, &d, 12), ACH_BULLSEYE));
  s.distance_m = 1500;
  assert(HAS(run(&s, &st, &d, 12), ACH_BULLSEYE));
  s.steps = 5999;
  assert(!HAS(achievements_compute(&s, &st, &d, &steps, 12, 20000), ACH_BULLSEYE));
  s.steps = 6000;
  assert(HAS(achievements_compute(&s, &st, &d, &steps, 12, 20000), ACH_BULLSEYE));
  st.last_end = T0 - 7 * DAY;
  assert(HAS(run(&s, &st, &d, 12), ACH_WELCOME_BACK));
  st.last_end = T0 - 6 * DAY;
  assert(!HAS(run(&s, &st, &d, 12), ACH_WELCOME_BACK));
}

static void test_week_and_usage(void) {
  JournalSession s = base();
  JournalStats st;
  JournalDays d;
  uint16_t m;
  memset(&st, 0, sizeof(st));
  memset(&d, 0, sizeof(d));
  d.days[0].day = 19994;
  d.days[0].distance_m = 40000;
  d.days[0].sessions = 4;
  s.distance_m = 10000;
  m = run(&s, &st, &d, 12);
  assert(HAS(m, ACH_ENTHUSIAST) && HAS(m, ACH_HOOKED));
  d.days[0].day = 19993;  // hors fenetre
  m = run(&s, &st, &d, 12);
  assert(!HAS(m, ACH_ENTHUSIAST) && !HAS(m, ACH_HOOKED));
  d.days[0].day = 19994;
  d.days[0].distance_m = 50000;
  d.days[0].sessions = 5;  // seuil deja franchi
  m = run(&s, &st, &d, 12);
  assert(!HAS(m, ACH_ENTHUSIAST) && !HAS(m, ACH_HOOKED));
  st.first_use = T0 - 29 * DAY;
  assert(!HAS(run(&s, &st, &d, 12), ACH_ONE_MONTH));
  st.first_use = T0 - 30 * DAY;
  assert(HAS(run(&s, &st, &d, 12), ACH_ONE_MONTH));
  st.unlocked = 1u << ACH_ONE_MONTH;
  assert(!HAS(run(&s, &st, &d, 12), ACH_ONE_MONTH));
  st.first_use = T0 - 365 * DAY;
  assert(HAS(run(&s, &st, &d, 12), ACH_VETERAN));
}

int main(void) {
  assert(journal_day_number(1970, 1, 1) == 0);
  assert(journal_day_number(2000, 1, 1) == 10957);
  assert(journal_day_number(2026, 3, 1) - journal_day_number(2026, 2, 28) == 1);
  assert(journal_day_number(2028, 3, 1) - journal_day_number(2028, 2, 28) == 2);
  assert(journal_day_number(2027, 1, 1) - journal_day_number(2026, 12, 31) == 1);
  test_simple();
  test_session();
  test_week_and_usage();
  printf("OK : achievements\n");
  return 0;
}
