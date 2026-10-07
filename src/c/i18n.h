#pragma once

#include <pebble.h>

// Identifiants des textes de l'application. Les trois premiers doivent
// rester dans l'ordre des activites (marche, course, entrainement).
typedef enum {
  STR_ACTIVITY_WALK = 0,
  STR_ACTIVITY_RUN,
  STR_ACTIVITY_TRAINING,
  STR_TOO_SHORT,
  STR_MIN_REQUIRED,
  STR_SESSION_DONE,
  STR_STOP_QUESTION,
  STR_SENSOR_UNAVAILABLE,
  STR_UNIT_STEPS,
  STR_LABEL_DURATION,
  STR_LABEL_STEPS,
  STR_LABEL_DISTANCE,
  STR_LABEL_HEART_RATE,
  STR_LABEL_PACE,
  STR_LABEL_SPEED,
  STR_SUMMARY_PACE,
  STR_SUMMARY_SPEED,
  STR_SUMMARY_HR_AVG,
  STR_SUMMARY_HR_MAX,
  STR_SUMMARY_CALORIES,
  STR_EFFORT_QUESTION,
  STR_EFFORT_RELAXED,
  STR_EFFORT_MODERATE,
  STR_EFFORT_DEMANDING,
  STR_EFFORT_EXHAUSTING,
  STR_KEEP_GOING,
  STR_COUNT
} StringId;

// Renvoie le texte dans la langue du systeme de la montre (anglais par defaut).
const char *tr(StringId id);
