#include "i18n.h"

#include <string.h>

// -1 = langue du systeme de la montre (comportement normal)
// Pour tester dans l'emulateur : 0 anglais, 1 francais, 2 allemand,
// 3 espagnol, 4 italien, 5 portugais.
#define I18N_FORCED_LANGUAGE -1

typedef enum {
  LANGUAGE_EN = 0,
  LANGUAGE_FR,
  LANGUAGE_DE,
  LANGUAGE_ES,
  LANGUAGE_IT,
  LANGUAGE_PT,
  LANGUAGE_COUNT
} Language;

static const char *const s_strings[LANGUAGE_COUNT][STR_COUNT] = {
  [LANGUAGE_EN] = {
    [STR_ACTIVITY_WALK]      = "Walk",
    [STR_ACTIVITY_RUN]       = "Run",
    [STR_ACTIVITY_TRAINING]  = "Workout",
    [STR_TOO_SHORT]          = "Session too short",
    [STR_MIN_REQUIRED]       = "Minimum required: 2 min",
    [STR_SESSION_DONE]       = "Session complete",
    [STR_STOP_QUESTION]      = "End workout?",
    [STR_SENSOR_UNAVAILABLE] = "Sensor unavailable",
    [STR_UNIT_STEPS]         = "steps",
    [STR_LABEL_DURATION]     = "DURATION",
    [STR_LABEL_STEPS]        = "STEPS",
    [STR_LABEL_DISTANCE]     = "DISTANCE",
    [STR_LABEL_HEART_RATE]   = "HEART RATE",
    [STR_LABEL_PACE]         = "AVG PACE",
    [STR_LABEL_SPEED]        = "AVG SPEED",
    [STR_SUMMARY_PACE]       = "AVERAGE PACE",
    [STR_SUMMARY_SPEED]      = "AVERAGE SPEED",
    [STR_SUMMARY_HR_AVG]     = "AVG HEART RATE",
    [STR_SUMMARY_HR_MAX]     = "MAX HEART RATE",
    [STR_SUMMARY_CALORIES]   = "CALORIES",
    [STR_EFFORT_QUESTION]    = "HOW WAS YOUR EFFORT?",
    [STR_EFFORT_RELAXED]     = "Relaxed",
    [STR_EFFORT_MODERATE]    = "Moderate",
    [STR_EFFORT_DEMANDING]   = "Demanding",
    [STR_EFFORT_EXHAUSTING]  = "Exhausting",
    [STR_KEEP_GOING]         = "Keep going",
    [STR_SUMMARY_EFFORT]     = "EFFORT",
  },
  [LANGUAGE_FR] = {
    [STR_ACTIVITY_WALK]      = "Marche",
    [STR_ACTIVITY_RUN]       = "Course",
    [STR_ACTIVITY_TRAINING]  = "Entraînement",
    [STR_TOO_SHORT]          = "Durée trop courte",
    [STR_MIN_REQUIRED]       = "Minimum requis : 2 min",
    [STR_SESSION_DONE]       = "Séance terminée",
    [STR_STOP_QUESTION]      = "Terminer l'exercice ?",
    [STR_SENSOR_UNAVAILABLE] = "Capteur indisponible",
    [STR_UNIT_STEPS]         = "pas",
    [STR_LABEL_DURATION]     = "DURÉE",
    [STR_LABEL_STEPS]        = "PAS",
    [STR_LABEL_DISTANCE]     = "DISTANCE",
    [STR_LABEL_HEART_RATE]   = "FRÉQUENCE CARD.",
    [STR_LABEL_PACE]         = "ALLURE MOY.",
    [STR_LABEL_SPEED]        = "VITESSE MOY.",
    [STR_SUMMARY_PACE]       = "ALLURE MOYENNE",
    [STR_SUMMARY_SPEED]      = "VITESSE MOYENNE",
    [STR_SUMMARY_HR_AVG]     = "FRÉQ. CARD. MOYENNE",
    [STR_SUMMARY_HR_MAX]     = "FRÉQ. CARD. MAX",
    [STR_SUMMARY_CALORIES]   = "CALORIES",
    [STR_EFFORT_QUESTION]    = "EFFORT RESSENTI ?",
    [STR_EFFORT_RELAXED]     = "Tranquille",
    [STR_EFFORT_MODERATE]    = "Modéré",
    [STR_EFFORT_DEMANDING]   = "Soutenu",
    [STR_EFFORT_EXHAUSTING]  = "Épuisant",
    [STR_KEEP_GOING]         = "Continuer",
    [STR_SUMMARY_EFFORT]     = "EFFORT",
  },
  [LANGUAGE_DE] = {
    [STR_ACTIVITY_WALK]      = "Gehen",
    [STR_ACTIVITY_RUN]       = "Laufen",
    [STR_ACTIVITY_TRAINING]  = "Training",
    [STR_TOO_SHORT]          = "Einheit zu kurz",
    [STR_MIN_REQUIRED]       = "Mindestens 2 Min. erforderlich",
    [STR_SESSION_DONE]       = "Einheit beendet",
    [STR_STOP_QUESTION]      = "Training beenden?",
    [STR_SENSOR_UNAVAILABLE] = "Sensor nicht verfügbar",
    [STR_UNIT_STEPS]         = "Schritte",
    [STR_LABEL_DURATION]     = "DAUER",
    [STR_LABEL_STEPS]        = "SCHRITTE",
    [STR_LABEL_DISTANCE]     = "STRECKE",
    [STR_LABEL_HEART_RATE]   = "HERZFREQUENZ",
    [STR_LABEL_PACE]         = "MITTL. TEMPO",
    [STR_LABEL_SPEED]        = "MITTL. GESCHW.",
    [STR_SUMMARY_PACE]       = "DURCHSCHN. TEMPO",
    [STR_SUMMARY_SPEED]      = "DURCHSCHN. GESCHW.",
    [STR_SUMMARY_HR_AVG]     = "MITTL. HERZFREQ.",
    [STR_SUMMARY_HR_MAX]     = "MAX. HERZFREQ.",
    [STR_SUMMARY_CALORIES]   = "KALORIEN",
    [STR_EFFORT_QUESTION]    = "WIE ANSTRENGEND?",
    [STR_EFFORT_RELAXED]     = "Entspannt",
    [STR_EFFORT_MODERATE]    = "Moderat",
    [STR_EFFORT_DEMANDING]   = "Anstrengend",
    [STR_EFFORT_EXHAUSTING]  = "Erschöpfend",
    [STR_KEEP_GOING]         = "Weitermachen",
    [STR_SUMMARY_EFFORT]     = "ANSTRENGUNG",
  },
  [LANGUAGE_ES] = {
    [STR_ACTIVITY_WALK]      = "Caminar",
    [STR_ACTIVITY_RUN]       = "Correr",
    [STR_ACTIVITY_TRAINING]  = "Entrenamiento",
    [STR_TOO_SHORT]          = "Sesión demasiado corta",
    [STR_MIN_REQUIRED]       = "Mínimo requerido: 2 min",
    [STR_SESSION_DONE]       = "Sesión terminada",
    [STR_STOP_QUESTION]      = "¿Terminar el ejercicio?",
    [STR_SENSOR_UNAVAILABLE] = "Sensor no disponible",
    [STR_UNIT_STEPS]         = "pasos",
    [STR_LABEL_DURATION]     = "DURACIÓN",
    [STR_LABEL_STEPS]        = "PASOS",
    [STR_LABEL_DISTANCE]     = "DISTANCIA",
    [STR_LABEL_HEART_RATE]   = "FREC. CARDÍACA",
    [STR_LABEL_PACE]         = "RITMO MEDIO",
    [STR_LABEL_SPEED]        = "VEL. MEDIA",
    [STR_SUMMARY_PACE]       = "RITMO MEDIO",
    [STR_SUMMARY_SPEED]      = "VELOCIDAD MEDIA",
    [STR_SUMMARY_HR_AVG]     = "FREC. CARD. MEDIA",
    [STR_SUMMARY_HR_MAX]     = "FREC. CARD. MÁX.",
    [STR_SUMMARY_CALORIES]   = "CALORÍAS",
    [STR_EFFORT_QUESTION]    = "¿CÓMO FUE EL ESFUERZO?",
    [STR_EFFORT_RELAXED]     = "Relajado",
    [STR_EFFORT_MODERATE]    = "Moderado",
    [STR_EFFORT_DEMANDING]   = "Exigente",
    [STR_EFFORT_EXHAUSTING]  = "Agotador",
    [STR_KEEP_GOING]         = "Continuar",
    [STR_SUMMARY_EFFORT]     = "ESFUERZO",
  },
  [LANGUAGE_IT] = {
    [STR_ACTIVITY_WALK]      = "Camminata",
    [STR_ACTIVITY_RUN]       = "Corsa",
    [STR_ACTIVITY_TRAINING]  = "Allenamento",
    [STR_TOO_SHORT]          = "Sessione troppo breve",
    [STR_MIN_REQUIRED]       = "Minimo richiesto: 2 min",
    [STR_SESSION_DONE]       = "Sessione terminata",
    [STR_STOP_QUESTION]      = "Terminare l'allenamento?",
    [STR_SENSOR_UNAVAILABLE] = "Sensore non disponibile",
    [STR_UNIT_STEPS]         = "passi",
    [STR_LABEL_DURATION]     = "DURATA",
    [STR_LABEL_STEPS]        = "PASSI",
    [STR_LABEL_DISTANCE]     = "DISTANZA",
    [STR_LABEL_HEART_RATE]   = "FREQ. CARDIACA",
    [STR_LABEL_PACE]         = "PASSO MEDIO",
    [STR_LABEL_SPEED]        = "VEL. MEDIA",
    [STR_SUMMARY_PACE]       = "PASSO MEDIO",
    [STR_SUMMARY_SPEED]      = "VELOCITÀ MEDIA",
    [STR_SUMMARY_HR_AVG]     = "FREQ. CARD. MEDIA",
    [STR_SUMMARY_HR_MAX]     = "FREQ. CARD. MAX",
    [STR_SUMMARY_CALORIES]   = "CALORIE",
    [STR_EFFORT_QUESTION]    = "COM'È STATO LO SFORZO?",
    [STR_EFFORT_RELAXED]     = "Rilassato",
    [STR_EFFORT_MODERATE]    = "Moderato",
    [STR_EFFORT_DEMANDING]   = "Impegnativo",
    [STR_EFFORT_EXHAUSTING]  = "Estenuante",
    [STR_KEEP_GOING]         = "Continua",
    [STR_SUMMARY_EFFORT]     = "SFORZO",
  },
  [LANGUAGE_PT] = {
    [STR_ACTIVITY_WALK]      = "Caminhada",
    [STR_ACTIVITY_RUN]       = "Corrida",
    [STR_ACTIVITY_TRAINING]  = "Treino",
    [STR_TOO_SHORT]          = "Sessão demasiado curta",
    [STR_MIN_REQUIRED]       = "Mínimo exigido: 2 min",
    [STR_SESSION_DONE]       = "Sessão terminada",
    [STR_STOP_QUESTION]      = "Terminar o treino?",
    [STR_SENSOR_UNAVAILABLE] = "Sensor indisponível",
    [STR_UNIT_STEPS]         = "passos",
    [STR_LABEL_DURATION]     = "DURAÇÃO",
    [STR_LABEL_STEPS]        = "PASSOS",
    [STR_LABEL_DISTANCE]     = "DISTÂNCIA",
    [STR_LABEL_HEART_RATE]   = "FREQ. CARDÍACA",
    [STR_LABEL_PACE]         = "RITMO MÉDIO",
    [STR_LABEL_SPEED]        = "VEL. MÉDIA",
    [STR_SUMMARY_PACE]       = "RITMO MÉDIO",
    [STR_SUMMARY_SPEED]      = "VELOCIDADE MÉDIA",
    [STR_SUMMARY_HR_AVG]     = "FREQ. CARD. MÉDIA",
    [STR_SUMMARY_HR_MAX]     = "FREQ. CARD. MÁX.",
    [STR_SUMMARY_CALORIES]   = "CALORIAS",
    [STR_EFFORT_QUESTION]    = "COMO FOI O ESFORÇO?",
    [STR_EFFORT_RELAXED]     = "Relaxado",
    [STR_EFFORT_MODERATE]    = "Moderado",
    [STR_EFFORT_DEMANDING]   = "Exigente",
    [STR_EFFORT_EXHAUSTING]  = "Exaustivo",
    [STR_KEEP_GOING]         = "Continuar",
    [STR_SUMMARY_EFFORT]     = "ESFORÇO",
  },
};

// La montre renvoie un code du type "fr_FR" : on ne regarde que la langue.
static Language language_from_locale(const char *locale) {
  if (locale == NULL) {
    return LANGUAGE_EN;
  }

  if (strncmp(locale, "fr", 2) == 0) return LANGUAGE_FR;
  if (strncmp(locale, "de", 2) == 0) return LANGUAGE_DE;
  if (strncmp(locale, "es", 2) == 0) return LANGUAGE_ES;
  if (strncmp(locale, "it", 2) == 0) return LANGUAGE_IT;
  if (strncmp(locale, "pt", 2) == 0) return LANGUAGE_PT;

  // Anglais par defaut. Le chinois n'est pas encore pris en charge :
  // il tombe aussi sur l'anglais.
  return LANGUAGE_EN;
}

static Language current_language(void) {
#if I18N_FORCED_LANGUAGE >= 0
  return (Language) I18N_FORCED_LANGUAGE;
#else
  return language_from_locale(i18n_get_system_locale());
#endif
}

const char *tr(StringId id) {
  const char *text = s_strings[current_language()][id];

  if (text == NULL) {
    text = s_strings[LANGUAGE_EN][id];
  }

  return text;
}
