#include <pebble.h>

#include <string.h>

#include "journal_persist.h"

static int read_block(uint32_t key, uint8_t *buf, int size) {
  if (!persist_exists(key)) {
    return 0;
  }

  return persist_read_data(key, buf, size);
}

void journal_load_index(JournalIndex *i) {
  uint8_t b[JOURNAL_INDEX_SIZE];

  i->next = 0;
  i->count = 0;
  journal_index_decode(b, read_block(JOURNAL_INDEX_KEY, b, sizeof(b)), i);
}

void journal_load_days(JournalDays *d) {
  uint8_t b[JOURNAL_DAYS_SIZE];

  memset(d, 0, sizeof(*d));
  journal_days_decode(b, read_block(JOURNAL_DAYS_KEY, b, sizeof(b)), d);
}

void journal_load_stats(JournalStats *st) {
  uint8_t b[JOURNAL_STATS_SIZE];

  memset(st, 0, sizeof(*st));
  journal_stats_decode(b, read_block(JOURNAL_STATS_KEY, b, sizeof(b)), st);
}

bool journal_load_session(int slot, JournalSession *s) {
  uint8_t b[JOURNAL_RECORD_SIZE];

  if (slot < 0 || slot >= JOURNAL_SLOTS) {
    return false;
  }

  return journal_decode(
      b, read_block(JOURNAL_SLOT_KEY_BASE + slot, b, sizeof(b)), s);
}

static bool write_block(uint32_t key, const uint8_t *buf, int size) {
  return persist_write_data(key, buf, size) == size;
}

bool journal_save_all(const JournalSession *s, int day) {
  uint8_t record[JOURNAL_RECORD_SIZE];
  uint8_t small[JOURNAL_DAYS_SIZE];
  JournalIndex index;
  JournalDays days;
  JournalStats stats;

  journal_load_index(&index);
  journal_load_days(&days);
  journal_load_stats(&stats);

  int slot = journal_index_new_slot(&index);
  int size = journal_encode(s, record, sizeof(record));

  if (size == 0 || !write_block(JOURNAL_SLOT_KEY_BASE + slot, record, size)) {
    return false;
  }

  journal_days_add(&days, day, s->distance_m);
  size = journal_days_encode(&days, small, sizeof(small));
  if (size == 0 || !write_block(JOURNAL_DAYS_KEY, small, size)) {
    return false;
  }

  journal_stats_commit(&stats, s);
  size = journal_stats_encode(&stats, small, sizeof(small));
  if (size == 0 || !write_block(JOURNAL_STATS_KEY, small, size)) {
    return false;
  }

  journal_index_advance(&index);
  size = journal_index_encode(&index, small, sizeof(small));
  return size != 0 && write_block(JOURNAL_INDEX_KEY, small, size);
}
