#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H

#include <stdint.h>

#include "journal.h"

#define DAYS_WINDOW 7
#define DAY_SECONDS 86400u

typedef struct {
  uint16_t day;  // numero du jour, 0 = emplacement vide
  uint32_t distance_m;
  uint8_t sessions;
} JournalDay;

typedef struct { JournalDay days[DAYS_WINDOW]; } JournalDays;

typedef struct {
  uint32_t first_use;  // 0 = jamais utilise
  uint32_t last_end;   // fin de la derniere seance enregistree
  uint32_t unlocked;   // succes obtenus une fois (bits ACH_*)
} JournalStats;

typedef struct {
  uint8_t goal_is_steps;  // 0 = distance en metres, 1 = pas
  uint32_t session_goal;
  uint32_t week_distance_m;
  uint16_t week_sessions;
} JournalGoals;

int journal_day_number(int year, int month, int day);
uint16_t achievements_compute(const JournalSession *s,
                              const JournalStats *st, const JournalDays *days,
                              const JournalGoals *g, int start_hour,
                              int day_number);

#endif
