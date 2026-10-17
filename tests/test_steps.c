#include <assert.h>
#include <stdio.h>

#include "../src/c/steps.h"

int main(void) {
  StepCounter c;

  step_counter_start(&c, 1000);
  assert(step_counter_update(&c, 1000) == 0);
  assert(step_counter_update(&c, 1500) == 500);
  assert(step_counter_update(&c, 20) == 520);   // passage de minuit
  assert(step_counter_update(&c, 100) == 600);
  assert(step_counter_update(&c, 5) == 605);    // second passage de minuit

  step_counter_start(&c, 0);
  assert(step_counter_update(&c, 50) == 50);

  step_counter_start(&c, -5);                   // valeur invalide
  assert(step_counter_update(&c, 10) == 10);

  // Pause : conserve les pas actifs et ignore ceux de la pause.
  step_counter_start(&c, 1000);
  assert(step_counter_update(&c, 1100) == 100);
  step_counter_rebase(&c, 1150);
  assert(step_counter_update(&c, 1150) == 100);
  assert(step_counter_update(&c, 1180) == 130);

  // Deuxieme pause, puis passage de minuit en activite.
  step_counter_rebase(&c, 1200);
  assert(step_counter_update(&c, 1210) == 140);
  assert(step_counter_update(&c, 10) == 150);

  // Pause traversant minuit : le nouveau total devient la reference.
  step_counter_start(&c, 2000);
  assert(step_counter_update(&c, 2100) == 100);
  step_counter_rebase(&c, 20);
  assert(step_counter_update(&c, 20) == 100);
  assert(step_counter_update(&c, 45) == 125);

  // Une nouvelle seance ne conserve pas les pas de la precedente.
  step_counter_start(&c, 45);
  assert(step_counter_update(&c, 55) == 10);

  printf("OK : steps\n");
  return 0;
}
