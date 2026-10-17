#include "steps.h"

void step_counter_start(StepCounter *counter, int total_today) {
  if (total_today < 0) {
    total_today = 0;
  }

  counter->start_total = total_today;
  counter->last_total = total_today;
  counter->banked = 0;
}

int step_counter_update(StepCounter *counter, int total_today) {
  if (total_today < 0) {
    total_today = 0;
  }

  if (total_today < counter->last_total) {
    counter->banked += counter->last_total - counter->start_total;
    counter->start_total = 0;
  }

  counter->last_total = total_today;
  return counter->banked + total_today - counter->start_total;
}

void step_counter_rebase(StepCounter *counter, int total_today) {
  int preserved =
      counter->banked + counter->last_total - counter->start_total;

  if (total_today < 0) {
    total_today = 0;
  }

  counter->banked = preserved;
  counter->start_total = total_today;
  counter->last_total = total_today;
}
