#pragma once

// Compteur de pas d'une seance. Le total du jour (Health) repart de zero a
// minuit : on met alors de cote les pas deja comptes.
typedef struct {
  int start_total;  // total du jour au demarrage de la seance
  int last_total;   // derniere valeur lue
  int banked;       // pas comptes avant un passage de minuit
} StepCounter;

void step_counter_start(StepCounter *counter, int total_today);

// Renvoie les pas de la seance.
int step_counter_update(StepCounter *counter, int total_today);

// Conserve les pas deja acquis et ignore les pas depuis la derniere lecture.
// A appeler a la reprise, apres une lecture normale a l'entree en pause.
void step_counter_rebase(StepCounter *counter, int total_today);
