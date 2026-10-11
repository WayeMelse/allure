#include "achievements.h"

#define BIT(a) ((uint16_t) (1u << (a)))

int journal_day_number(int year, int month, int day) {
  int y = year - (month <= 2);
  int era = (y >= 0 ? y : y - 399) / 400;
  int yoe = y - era * 400;
  int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static bool in_window(uint16_t day, int today) {
  return day != 0 && day <= today && day + (DAYS_WINDOW - 1) >= today;
}

static void once(uint16_t *r, const JournalStats *st, int ach,
                 uint32_t used_days, uint32_t needed_days) {
  if (used_days >= needed_days && !(st->unlocked & (1u << ach))) {
    *r |= BIT(ach);
  }
}

uint16_t achievements_compute(const JournalSession *s,
                              const JournalStats *st, const JournalDays *days,
                              const JournalGoals *g, int start_hour,
                              int day_number) {
  uint16_t r = 0;
  uint32_t before_m = 0;
  uint32_t before_n = 0;

  for (int i = 0; i < DAYS_WINDOW; i++) {
    if (in_window(days->days[i].day, day_number)) {
      before_m += days->days[i].distance_m;
      before_n += days->days[i].sessions;
    }
  }

  if (st->last_end != 0 && s->start_time >= st->last_end &&
      s->start_time - st->last_end >= 7 * DAY_SECONDS) {
    r |= BIT(ACH_WELCOME_BACK);
  }
  if (s->active_seconds > 600) r |= BIT(ACH_WARMED_UP);
  if (start_hour < 7) r |= BIT(ACH_EARLY_BIRD);
  if (start_hour >= 20) r |= BIT(ACH_NIGHT_OWL);

  uint32_t reached = g->goal_is_steps ? s->steps : s->distance_m;
  if (g->session_goal > 0 && reached >= g->session_goal) {
    r |= BIT(ACH_BULLSEYE);
  }

  if (g->week_distance_m > 0 && before_m < g->week_distance_m &&
      before_m + s->distance_m >= g->week_distance_m) {
    r |= BIT(ACH_ENTHUSIAST);
  }
  if (g->week_sessions > 0 && before_n < g->week_sessions &&
      before_n + 1 >= g->week_sessions) {
    r |= BIT(ACH_HOOKED);
  }

  if (s->calories >= 1000) {
    r |= BIT(ACH_FURNACE);
  } else if (s->calories >= 500) {
    r |= BIT(ACH_FIRE_STARTER);
  }
  if (s->redline_seconds > 300) r |= BIT(ACH_REDLINE);

  if (st->first_use != 0 && s->start_time > st->first_use) {
    uint32_t used = (s->start_time - st->first_use) / DAY_SECONDS;
    once(&r, st, ACH_ONE_MONTH, used, 30);
    once(&r, st, ACH_LOYAL, used, 180);
    once(&r, st, ACH_VETERAN, used, 365);
  }

  return r;
}
