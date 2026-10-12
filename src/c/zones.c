#include "zones.h"

#define ZONES_MIN_RESERVE_BPM 40

// Pourcentages de la frequence maximale (methode actuelle).
static const int s_max_percents[3][4] = {
  {53, 64, 75, 86}, {60, 70, 80, 90}, {60, 70, 80, 90}
};

// Pourcentages de la reserve cardiaque (Karvonen), calibres pour retrouver
// les zones ci-dessus avec une frequence de repos de 60 bpm.
static const int s_reserve_percents[3][4] = {
  {30, 46, 63, 79}, {40, 55, 70, 85}, {40, 55, 70, 85}
};

int zones_upper_limit_bpm(int activity, int zone, int max_bpm,
                          int resting_hr) {
  if (activity < 0 || activity > 2) {
    activity = 0;
  }
  if (zone < 0) {
    zone = 0;
  }
  if (zone > 3) {
    zone = 3;
  }

  if (resting_hr > 0 && max_bpm - resting_hr >= ZONES_MIN_RESERVE_BPM) {
    return resting_hr +
           ((max_bpm - resting_hr) * s_reserve_percents[activity][zone]) / 100;
  }

  return (max_bpm * s_max_percents[activity][zone]) / 100;
}
