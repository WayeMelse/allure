#ifndef JOURNAL_PERSIST_H
#define JOURNAL_PERSIST_H

#include <stdbool.h>

#include "journal_store.h"

// Cles de stockage persistant (1, 3, 4, 5 sont deja prises ; 2 est abandonnee).
#define JOURNAL_INDEX_KEY 6
#define JOURNAL_SLOT_KEY_BASE 10
#define JOURNAL_STATS_KEY 20
#define JOURNAL_DAYS_KEY 21

void journal_load_index(JournalIndex *i);
void journal_load_days(JournalDays *d);
void journal_load_stats(JournalStats *st);
bool journal_load_session(int slot, JournalSession *s);

// Enregistre la seance (s->achievements deja rempli), met a jour le tableau
// des jours et les statistiques, puis l'index en dernier.
bool journal_save_all(const JournalSession *s, int day);

#endif
