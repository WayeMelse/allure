#ifndef ZONES_H
#define ZONES_H

// Activites : 0 = marche, 1 = course, 2 = entrainement.
// Limite superieure en bpm des zones 1 a 4 (indices 0 a 3).
// Si resting_hr > 0 et que la reserve (max - repos) fait au moins 40 bpm :
// methode de Karvonen. Sinon : pourcentage de la frequence maximale.
int zones_upper_limit_bpm(int activity, int zone, int max_bpm, int resting_hr);

#endif
