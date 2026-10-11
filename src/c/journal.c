#include "journal.h"

#include <string.h>

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

int journal_encode(const JournalSession *s, uint8_t *b, int capacity) {
  if (capacity < JOURNAL_RECORD_SIZE) {
    return 0;
  }

  memset(b, 0, JOURNAL_RECORD_SIZE);
  b[0] = JOURNAL_VERSION;
  b[1] = s->activity;
  put32(b + 2, s->start_time);
  put16(b + 6, s->active_seconds);
  put16(b + 8, s->total_seconds);
  put32(b + 10, s->steps);
  put32(b + 14, s->distance_m);
  put16(b + 18, s->calories);
  b[20] = s->avg_bpm;
  b[21] = s->max_bpm;
  b[22] = s->avg_zone_x10;
  b[23] = s->effort_score;
  b[24] = s->feeling;
  b[25] = s->level;
  for (int z = 0; z < JOURNAL_ZONE_COUNT; z++) {
    put16(b + 26 + 2 * z, s->zone_seconds[z]);
  }

  put16(b + 36, s->redline_seconds);
  put16(b + 38, s->achievements);

  int count = s->event_count;
  if (count > JOURNAL_MAX_EVENTS) {
    count = JOURNAL_MAX_EVENTS;
  }
  b[40] = (uint8_t) count;

  for (int i = 0; i < count; i++) {
    put16(b + 41 + 4 * i, s->events[i].seconds);
    b[43 + 4 * i] = s->events[i].type;
    b[44 + 4 * i] = s->events[i].value;
  }

  return JOURNAL_RECORD_SIZE;
}

bool journal_decode(const uint8_t *b, int length, JournalSession *s) {
  if (length < JOURNAL_RECORD_SIZE || b[0] != JOURNAL_VERSION ||
      b[40] > JOURNAL_MAX_EVENTS) {
    return false;
  }

  memset(s, 0, sizeof(*s));
  s->activity = b[1];
  s->start_time = get32(b + 2);
  s->active_seconds = get16(b + 6);
  s->total_seconds = get16(b + 8);
  s->steps = get32(b + 10);
  s->distance_m = get32(b + 14);
  s->calories = get16(b + 18);
  s->avg_bpm = b[20];
  s->max_bpm = b[21];
  s->avg_zone_x10 = b[22];
  s->effort_score = b[23];
  s->feeling = b[24];
  s->level = b[25];
  for (int z = 0; z < JOURNAL_ZONE_COUNT; z++) {
    s->zone_seconds[z] = get16(b + 26 + 2 * z);
  }

  s->redline_seconds = get16(b + 36);
  s->achievements = get16(b + 38);
  s->event_count = b[40];

  for (int i = 0; i < s->event_count; i++) {
    s->events[i].seconds = get16(b + 41 + 4 * i);
    s->events[i].type = b[43 + 4 * i];
    s->events[i].value = b[44 + 4 * i];
  }

  return true;
}

bool journal_add_event(JournalSession *s, uint8_t type,
                       uint16_t seconds, uint8_t value) {
  static const int budgets[3] = {
    JOURNAL_MAX_ZONE_EVENTS, JOURNAL_MAX_BORNE_EVENTS,
    JOURNAL_MAX_FLAME_EVENTS
  };
  int used = 0;

  if (type > JE_FLAME || s->event_count >= JOURNAL_MAX_EVENTS) {
    return false;
  }

  for (int i = 0; i < s->event_count; i++) {
    if (s->events[i].type == type) {
      used++;
    }
  }

  if (used >= budgets[type]) {
    return false;
  }

  s->events[s->event_count].seconds = seconds;
  s->events[s->event_count].type = type;
  s->events[s->event_count].value = value;
  s->event_count++;
  return true;
}

int journal_top_achievement(uint16_t achievements) {
  for (int bit = ACH_COUNT - 1; bit >= 0; bit--) {
    if (achievements & (1u << bit)) {
      return bit;
    }
  }

  return -1;
}
