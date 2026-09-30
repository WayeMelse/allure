# Allure

Une application d'entraînement pour la **Pebble Time 2** (plateforme `emery`) :
marche, course ou entraînement, avec ta fréquence cardiaque affichée en direct
dans cinq zones d'effort en couleur.

English: [README.md](README.md)

## Fonctions

- Trois activités : marche, course, entraînement, avec un compte à rebours de
  3 secondes.
- Fréquence cardiaque en direct. La moitié haute de l'écran change de couleur
  selon la zone d'effort (cinq zones, en pourcentage de ta fréquence maximale
  estimée).
- Durée, pas, distance, allure et vitesse. Le bouton du bas change de métrique.
- Résumé de séance : fréquence cardiaque moyenne et maximale, calories estimées.
- Réglages sur le téléphone : sexe, âge, taille et poids.
- La langue suit celle de la montre : anglais, français, allemand, espagnol,
  italien et portugais.

## Comment les valeurs sont calculées

- Fréquence maximale : formule de Tanaka (208 - 0,7 x âge) pour les hommes,
  formule de Gulati (206 - 0,88 x âge) pour les femmes.
- Zones : moins de 60 %, 60-70 %, 70-80 %, 80-90 % et 90 % ou plus de ce maximum.
- Calories : formule de Keytel, à partir de la fréquence moyenne, du poids, de
  l'âge et du sexe.
- Distance : nombre de pas multiplié par une longueur de foulée estimée d'après
  ta taille. Il n'y a pas de GPS : distance, allure et vitesse sont des
  estimations.

## Prérequis

- Une Pebble Time 2 (capteur cardiaque indispensable).
- Le SDK Pebble, avec la plateforme `emery`.

## Compiler et installer

```bash
pebble package install @rebble/clay   # page de réglages (première fois)
pebble build
pebble install --emulator emery       # émulateur
pebble install --phone <ip-du-telephone>   # vraie montre, connexion développeur activée
```

## Réglages

Ouvre l'application dans l'application mobile Pebble et appuie sur la roue
dentée. Les valeurs sont conservées sur le téléphone et sur la montre. Tant que
tu ne les as pas enregistrées une fois, des valeurs par défaut sont utilisées
(homme, 35 ans, 175 cm, 75 kg).

## État et limites

- Testée sur une Pebble Time 2 et dans l'émulateur.
- Unités métriques uniquement (kg, cm, km).
- Les textes allemands, espagnols, italiens et portugais ont été écrits avec
  assistance et n'ont pas été relus par des locuteurs natifs : les corrections
  sont les bienvenues.
- Le chinois n'est pas encore pris en charge ; une montre en chinois affiche
  l'anglais.

## Licences

Le code source est publié sous [licence MIT](LICENSE).
Les polices de `resources/fonts/` gardent leur propre licence (SIL Open Font
License 1.1) : voir [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

Auteur : WayeM
