#ifndef JOURNAL_STORE_H
#define JOURNAL_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "achievements.h"

// Succes obtenus une seule fois (gardes dans les statistiques).
#define JOURNAL_ONCE_MASK \
  ((1u << ACH_ONE_MONTH) | (1u << ACH_LOYAL) | (1u << ACH_VETERAN))

typedef struct {
  uint8_t next;   // emplacement ou ecrire la prochaine seance (0 a 6)
  uint8_t count;  // seances enregistrees (0 a 7)
} JournalIndex;

bool journal_index_is_full(const JournalIndex *i);
int journal_index_new_slot(const JournalIndex *i);
void journal_index_advance(JournalIndex *i);
// age 0 = la plus recente. Renvoie -1 si absente.
int journal_index_slot_by_age(const JournalIndex *i, int age);

void journal_days_add(JournalDays *d, int day, uint32_t distance_m);
void journal_stats_commit(JournalStats *st, const JournalSession *s);

#endif
