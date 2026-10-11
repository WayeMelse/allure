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

  printf("OK : steps\n");
  return 0;
}
