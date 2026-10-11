#ifndef JOURNAL_H
#define JOURNAL_H

#include <stdbool.h>
#include <stdint.h>

#define JOURNAL_SLOTS 7
#define JOURNAL_VERSION 1
#define JOURNAL_RECORD_SIZE 249
#define JOURNAL_MAX_EVENTS 52
#define JOURNAL_ZONE_COUNT 5
#define JOURNAL_MAX_ZONE_EVENTS 18
#define JOURNAL_MAX_BORNE_EVENTS 20
#define JOURNAL_MAX_FLAME_EVENTS 14

typedef enum { JE_ZONE = 0, JE_BORNE, JE_FLAME } JournalEventType;

// Bit le plus haut = succes le plus difficile.
typedef enum {
  ACH_WELCOME_BACK = 0, ACH_WARMED_UP, ACH_EARLY_BIRD, ACH_NIGHT_OWL,
  ACH_BULLSEYE, ACH_ENTHUSIAST, ACH_FIRE_STARTER, ACH_HOOKED,
  ACH_REDLINE, ACH_FURNACE, ACH_ONE_MONTH, ACH_LOYAL, ACH_VETERAN,
  ACH_COUNT
} JournalAchievement;

typedef struct {
  uint16_t seconds;  // duree active depuis le debut
  uint8_t type;
  uint8_t value;
} JournalEvent;

typedef struct {
  uint8_t activity;
  uint32_t start_time;
  uint16_t active_seconds;
  uint16_t total_seconds;
  uint32_t steps;
  uint32_t distance_m;
  uint16_t calories;
  uint8_t avg_bpm;
  uint8_t max_bpm;
  uint8_t avg_zone_x10;
  uint8_t effort_score;
  uint8_t feeling;  // 0 = pas de reponse, 1 a 4
  uint8_t level;    // 0 = aucune donnee, 1 a 5
  uint16_t zone_seconds[JOURNAL_ZONE_COUNT];
  uint16_t redline_seconds;
  uint16_t achievements;
  uint8_t event_count;
  JournalEvent events[JOURNAL_MAX_EVENTS];
} JournalSession;

int journal_encode(const JournalSession *s, uint8_t *buf, int capacity);
bool journal_decode(const uint8_t *buf, int length, JournalSession *s);
bool journal_add_event(JournalSession *s, uint8_t type,
                       uint16_t seconds, uint8_t value);
int journal_top_achievement(uint16_t achievements);  // -1 si aucun

#endif
