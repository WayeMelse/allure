#include "journal_store.h"

bool journal_index_is_full(const JournalIndex *i) {
  return i->count >= JOURNAL_SLOTS;
}

int journal_index_new_slot(const JournalIndex *i) {
  return i->next % JOURNAL_SLOTS;
}

void journal_index_advance(JournalIndex *i) {
  i->next = (uint8_t) ((i->next + 1) % JOURNAL_SLOTS);
  if (i->count < JOURNAL_SLOTS) {
    i->count++;
  }
}

int journal_index_slot_by_age(const JournalIndex *i, int age) {
  if (age < 0 || age >= i->count) {
    return -1;
  }

  return (i->next + JOURNAL_SLOTS - 1 - age) % JOURNAL_SLOTS;
}

void journal_days_add(JournalDays *d, int day, uint32_t distance_m) {
  int free_slot = -1;
  int oldest = 0;

  for (int i = 0; i < DAYS_WINDOW; i++) {
    JournalDay *e = &d->days[i];

    if (e->day == day) {
      e->distance_m += distance_m;
      if (e->sessions < 255) {
        e->sessions++;
      }
      return;
    }

    if (free_slot < 0 && (e->day == 0 || e->day + (DAYS_WINDOW - 1) < day)) {
      free_slot = i;
    }

    if (e->day < d->days[oldest].day) {
      oldest = i;
    }
  }

  if (free_slot < 0) {
    free_slot = oldest;
  }

  d->days[free_slot].day = (uint16_t) day;
  d->days[free_slot].distance_m = distance_m;
  d->days[free_slot].sessions = 1;
}

void journal_stats_commit(JournalStats *st, const JournalSession *s) {
  if (st->first_use == 0) {
    st->first_use = s->start_time;
  }

  st->last_end = s->start_time + s->total_seconds;
  st->unlocked |= (uint32_t) (s->achievements & JOURNAL_ONCE_MASK);
}
