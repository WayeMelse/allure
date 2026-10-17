#include "zones.h"

#define ZONES_MIN_RESERVE_BPM 40

// Pourcentages de la frequence maximale (methode actuelle).
static const int s_max_percents[3][4] = {
  {53, 64, 75, 86}, {60, 70, 80, 90}, {60, 70, 80, 90}
};

// Calibration Allure sur la reserve cardiaque (formule de Karvonen).
// Valeurs en milliemes : calculs entiers uniquement.
// Marche : 15 / 35 / 55 / 75 %.
// Course et entrainement : 30 / 47,3 / 64,7 / 82 %.
// Limites intermediaires reparties regulierement, a l'arrondi pres.
static const int s_reserve_permille[3][4] = {
  {150, 350, 550, 750},
  {300, 473, 647, 820},
  {300, 473, 647, 820}
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
           ((max_bpm - resting_hr) * s_reserve_permille[activity][zone]) / 1000;
  }

  return (max_bpm * s_max_percents[activity][zone]) / 100;
}
