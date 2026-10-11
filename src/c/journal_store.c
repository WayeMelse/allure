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

static void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t) (v & 0xFF);
  p[1] = (uint8_t) (v >> 8);
}

static void put32(uint8_t *p, uint32_t v) {
  put16(p, (uint16_t) (v & 0xFFFF));
  put16(p + 2, (uint16_t) (v >> 16));
}

static uint16_t get16(const uint8_t *p) {
  return (uint16_t) (p[0] | (p[1] << 8));
}

static uint32_t get32(const uint8_t *p) {
  return get16(p) | ((uint32_t) get16(p + 2) << 16);
}

int journal_index_encode(const JournalIndex *i, uint8_t *b, int capacity) {
  if (capacity < JOURNAL_INDEX_SIZE) {
    return 0;
  }

  b[0] = JOURNAL_VERSION;
  b[1] = i->next;
  b[2] = i->count;
  return JOURNAL_INDEX_SIZE;
}

bool journal_index_decode(const uint8_t *b, int length, JournalIndex *i) {
  if (length < JOURNAL_INDEX_SIZE || b[0] != JOURNAL_VERSION ||
      b[1] >= JOURNAL_SLOTS || b[2] > JOURNAL_SLOTS) {
    return false;
  }

  i->next = b[1];
  i->count = b[2];
  return true;
}

int journal_days_encode(const JournalDays *d, uint8_t *b, int capacity) {
  if (capacity < JOURNAL_DAYS_SIZE) {
    return 0;
  }

  b[0] = JOURNAL_VERSION;
  for (int i = 0; i < DAYS_WINDOW; i++) {
    uint8_t *p = b + 1 + 7 * i;
    put16(p, d->days[i].day);
    put32(p + 2, d->days[i].distance_m);
    p[6] = d->days[i].sessions;
  }
  return JOURNAL_DAYS_SIZE;
}

bool journal_days_decode(const uint8_t *b, int length, JournalDays *d) {
  if (length < JOURNAL_DAYS_SIZE || b[0] != JOURNAL_VERSION) {
    return false;
  }

  for (int i = 0; i < DAYS_WINDOW; i++) {
    const uint8_t *p = b + 1 + 7 * i;
    d->days[i].day = get16(p);
    d->days[i].distance_m = get32(p + 2);
    d->days[i].sessions = p[6];
  }
  return true;
}

int journal_stats_encode(const JournalStats *st, uint8_t *b, int capacity) {
  if (capacity < JOURNAL_STATS_SIZE) {
    return 0;
  }

  b[0] = JOURNAL_VERSION;
  put32(b + 1, st->first_use);
  put32(b + 5, st->last_end);
  put32(b + 9, st->unlocked);
  return JOURNAL_STATS_SIZE;
}

bool journal_stats_decode(const uint8_t *b, int length, JournalStats *st) {
  if (length < JOURNAL_STATS_SIZE || b[0] != JOURNAL_VERSION) {
    return false;
  }

  st->first_use = get32(b + 1);
  st->last_end = get32(b + 5);
  st->unlocked = get32(b + 9);
  return true;
}
