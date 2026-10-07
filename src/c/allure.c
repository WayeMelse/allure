#include <pebble.h>
#include <stdlib.h>
#include "i18n.h"

typedef enum {
  ACTIVITY_WALK = 0,
  ACTIVITY_RUN,
  ACTIVITY_TRAINING,
  ACTIVITY_COUNT
} Activity;

typedef enum {
  METRIC_DURATION = 0,
  METRIC_STEPS,
  METRIC_DISTANCE,
  METRIC_HEART_RATE,
  METRIC_PACE,
  METRIC_SPEED,
  METRIC_COUNT
} WorkoutMetric;

#define MINIMUM_SESSION_SECONDS 120
// 1 = ligne de debug du score d'effort dans le resume (ne jamais commiter a 1).
#define EFFORT_DEBUG 0
#define SUMMARY_SCROLL_STEP 30
// Frequence de relecture du compteur de pas (et donc de la distance,
// de l'allure et de la vitesse, qui en decoulent). Arbitrairement fixee
// a 30 secondes : suffisant pour un suivi fluide sans solliciter le
// service Health trop souvent.
#define STEPS_REFRESH_INTERVAL_SECONDS 30
#define HEART_RATE_REFRESH_INTERVAL_SECONDS 2
// 1 = l'affichage et la couleur utilisent la mesure brute du capteur
// (reactive, un peu plus bruitee). 0 = mesure filtree par le systeme
// (moyennee sur la derniere minute, peut avoir du retard).
// Les statistiques de la seance (moyenne, max, calories) utilisent
// toujours la mesure filtree.
#define HEART_RATE_DISPLAY_USES_RAW 1

// Profil utilisateur : valeurs par defaut, avant tout reglage sur le
// telephone. Le profil reel est lu dans s_profile (voir plus bas).
#define DEFAULT_USER_IS_MALE 1
#define DEFAULT_USER_AGE_YEARS 35
#define DEFAULT_USER_HEIGHT_CM 175
#define DEFAULT_USER_WEIGHT_KG 75
#define PROFILE_PERSIST_KEY 1

// Jaune d'accent utilise pour la selection et les fonds d'ecrans
// secondaires. GColorYellow est plus vif/sature que GColorPastelYellow.
#define ACCENT_YELLOW GColorYellow
#define SECONDARY_TEXT_COLOR GColorDukeBlue  // bleu fonce, remplace le gris (jaune et blanc)

// Reglage du chiffre du compte a rebours (ecran Emery 200x228)
#define COUNTDOWN_LAYER_HEIGHT 130
#define COUNTDOWN_Y_ADJUST 0
#define WORKOUT_CLOCK_Y_ADJUST (-8)
#define SUMMARY_UNIT_Y_OFFSET 6

// Profil utilisateur, regle depuis le telephone (page Clay) et memorise
// sur la montre.
typedef struct {
  int16_t is_male;     // 1 = homme, 0 = femme
  int16_t age_years;
  int16_t height_cm;
  int16_t weight_kg;
} UserProfile;

static UserProfile s_profile = {
  .is_male = DEFAULT_USER_IS_MALE,
  .age_years = DEFAULT_USER_AGE_YEARS,
  .height_cm = DEFAULT_USER_HEIGHT_CM,
  .weight_kg = DEFAULT_USER_WEIGHT_KG
};

static GFont s_roboto_condensed_extrabold_font = NULL;
static GFont s_roboto_condensed_extrabold_countdown_font = NULL;
static GFont s_summary_font = NULL;
static GFont s_emoji_font = NULL;

static Window *s_menu_window;
static Window *s_countdown_window;
static Window *s_workout_window;
static Window *s_stop_confirm_window;
static Window *s_summary_window;

static Layer *s_menu_layer;
static Layer *s_workout_layer;
static Layer *s_stop_confirm_layer;
static Layer *s_summary_layer;

static TextLayer *s_countdown_activity_layer;
static TextLayer *s_countdown_number_layer;

static Activity s_selected_activity = ACTIVITY_WALK;
static WorkoutMetric s_current_metric = METRIC_DURATION;

static int s_countdown_value = 3;
static int s_elapsed_seconds = 0;

static bool s_is_paused = false;
static bool s_stay_awake = false;

static AppTimer *s_countdown_timer;

static char s_countdown_text[4];
static char s_clock_text[6];
static char s_metric_value_text[24];
static char s_metric_unit_text[12];
static char s_metric_label_text[24];

static int s_current_bpm = 0;
static bool s_heart_rate_available = false;
static int s_seconds_since_last_bpm_read = 0;

static int s_current_steps = 0;
static int s_steps_at_start = 0;
static int s_seconds_since_last_steps_read = 0;

// Statistiques accumulees sur toute la seance, pour le resume final
static long s_bpm_sum = 0;
static int s_bpm_sample_count = 0;
static int s_bpm_max = 0;
// Somme des zones (1 a 5) de chaque echantillon de FC de la seance.
static long s_zone_points_sum = 0;
static int get_raw_heart_rate_zone(int bpm);

// Effort ressenti, choisi sur l'ecran de fin de seance.
// s_effort_choice : ligne selectionnee (0 = continuer, 1 a 4 = niveau).
// s_effort_rating : niveau valide (0 = pas de reponse, 1 a 4).
static int s_effort_choice = 0;
static int s_effort_rating = 0;

// Defilement vertical de l'ecran de resume
static int s_summary_scroll_offset = 0;
static int s_summary_content_height = 0;

static void draw_pause_icon(GContext *ctx, int16_t x, int16_t y, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, GRect(x, y, 4, 14), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 8, y, 4, 14), 0, GCornerNone);
}

static void draw_play_icon(GContext *ctx, int16_t x, int16_t y, GColor color) {
  GPoint triangle_points[3] = {
    GPoint(x, y),
    GPoint(x, y + 14),
    GPoint(x + 12, y + 7)
  };

  GPathInfo triangle_info = {
    .num_points = 3,
    .points = triangle_points
  };

  GPath *triangle_path = gpath_create(&triangle_info);

  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, triangle_path);
  gpath_destroy(triangle_path);
}

static void draw_stop_icon(GContext *ctx, int16_t x, int16_t y, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, GRect(x, y, 14, 14), 0, GCornerNone);
}

static void draw_bulb_icon(GContext *ctx, int16_t x, int16_t y,
                             GColor color, bool filled) {
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);

  if (filled) {
    graphics_fill_circle(ctx, GPoint(x + 7, y + 6), 6);
  } else {
    graphics_draw_circle(ctx, GPoint(x + 7, y + 6), 6);

    graphics_draw_line(ctx, GPoint(x + 4, y + 5), GPoint(x + 7, y + 10));
    graphics_draw_line(ctx, GPoint(x + 7, y + 10), GPoint(x + 10, y + 5));
  }

  graphics_draw_line(ctx, GPoint(x + 4, y + 14), GPoint(x + 10, y + 14));
  graphics_draw_line(ctx, GPoint(x + 5, y + 17), GPoint(x + 9, y + 17));
}

static void draw_check_icon(GContext *ctx, int16_t x, int16_t y, GColor color) {
  // Coche vectorielle (deux segments), utilisee une seule fois, sur
  // le bouton haut de l'ecran "Terminer l'exercice ?".
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 3);

  graphics_draw_line(ctx, GPoint(x, y + 6), GPoint(x + 5, y + 11));
  graphics_draw_line(ctx, GPoint(x + 5, y + 11), GPoint(x + 15, y - 1));
}

// Emojis en UTF-8 : marche U+1F6B6, course U+1F3C3, coeur U+2665.
static void draw_activity_emoji(GContext *ctx, Activity activity,
                                GRect frame, GColor color) {
  const char *emoji;

  if (activity == ACTIVITY_WALK) {
    emoji = "\xF0\x9F\x9A\xB6";
  } else if (activity == ACTIVITY_RUN) {
    emoji = "\xF0\x9F\x8F\x83";
  } else {
    emoji = "\xE2\x99\xA5";
  }

  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(
      ctx,
      emoji,
      s_emoji_font,
      frame,
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
}

static void menu_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int16_t row_height = 56;
  const int16_t first_row_y = 12;

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  for (int i = 0; i < ACTIVITY_COUNT; i++) {
    int16_t row_y = first_row_y + i * row_height;
    bool selected = (i == s_selected_activity);

    if (selected) {
      graphics_context_set_fill_color(ctx, ACCENT_YELLOW);
      graphics_fill_rect(
          ctx,
          GRect(0, row_y, bounds.size.w, row_height),
          0,
          GCornerNone);
    }

    GColor icon_color = selected ? GColorBlack : GColorDarkGray;

    draw_activity_emoji(
        ctx,
        (Activity) i,
        GRect(8, row_y + 3, 60, 50),
        icon_color);

    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(
        ctx,
        tr(STR_ACTIVITY_WALK + (i)),
        fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(76, row_y + 14, bounds.size.w - 82, 32),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft,
        NULL);
  }
}

static void update_countdown_text(void) {
  snprintf(
      s_countdown_text,
      sizeof(s_countdown_text),
      "%d",
      s_countdown_value);

  text_layer_set_text(s_countdown_number_layer, s_countdown_text);
}

static void update_clock_text(void) {
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);

  strftime(
      s_clock_text,
      sizeof(s_clock_text),
      "%H:%M",
      current_time);
}

static void refresh_heart_rate_if_needed(void) {
  bool should_read =
      (s_seconds_since_last_bpm_read >= HEART_RATE_REFRESH_INTERVAL_SECONDS) ||
      !s_heart_rate_available;

  if (!should_read) {
    s_seconds_since_last_bpm_read++;
    return;
  }

  s_seconds_since_last_bpm_read = 0;

  int filtered_bpm = (int) health_service_peek_current_value(
      HealthMetricHeartRateBPM);

  int raw_bpm = (int) health_service_peek_current_value(
      HealthMetricHeartRateRawBPM);

  bool raw_valid = (raw_bpm >= 30 && raw_bpm <= 230);

  int display_bpm =
      (HEART_RATE_DISPLAY_USES_RAW && raw_valid) ? raw_bpm : filtered_bpm;

  if (display_bpm <= 0) {
    s_current_bpm = 0;
    s_heart_rate_available = false;
    return;
  }

  s_current_bpm = display_bpm;
  s_heart_rate_available = true;

  if (filtered_bpm > 0) {
    s_bpm_sum += filtered_bpm;
    s_bpm_sample_count++;
    s_zone_points_sum += get_raw_heart_rate_zone(filtered_bpm) + 1;

    if (filtered_bpm > s_bpm_max) {
      s_bpm_max = filtered_bpm;
    }
  }
}

static void refresh_steps_if_needed(void) {
  s_seconds_since_last_steps_read++;

  if (s_seconds_since_last_steps_read < STEPS_REFRESH_INTERVAL_SECONDS) {
    return;
  }

  s_seconds_since_last_steps_read = 0;

  // health_service_peek_current_value() n'est pas applicable aux metriques
  // cumulatives comme les pas (elle retourne toujours 0 pour ce type).
  // Il faut utiliser le total du jour, puis soustraire le total au demarrage
  // de la seance pour obtenir uniquement les pas de la seance en cours.
  HealthValue total_today = health_service_sum_today(HealthMetricStepCount);

  int steps_since_start = (int) total_today - s_steps_at_start;

  if (steps_since_start < 0) {
    steps_since_start = 0;
  }

  s_current_steps = steps_since_start;

}

static int get_stride_length_cm(void) {
  if (s_selected_activity == ACTIVITY_RUN) {
    return (s_profile.height_cm * 45) / 100;
  }

  return (s_profile.height_cm * 415) / 1000;
}

// Distance en metres, en entier : evite tout %f (non supporte par la
// libc embarquee du firmware Pebble, cause du bug "floating point").
static int get_distance_meters(void) {
  int stride_length_cm = get_stride_length_cm();
  return (s_current_steps * stride_length_cm) / 100;
}

static int get_average_bpm(void) {
  if (s_bpm_sample_count == 0) {
    return 0;
  }

  return (int) (s_bpm_sum / s_bpm_sample_count);
}

// Estime les calories brulees pendant la seance a partir de la formule de
// Keytel (heart-rate based), plus precise que le compteur natif Pebble sur
// des seances courtes. Necessite un point de mesure FC valide (moyenne
// de la seance). Calcul entierement en entiers 64 bits pour eviter tout
// %f (non supporte) et tout risque de depassement sur une longue seance.
//
// Hommes : kcal/min = (-55.0969 + 0.6309*HR + 0.1988*W + 0.2017*A) / 4.184
// Femmes : kcal/min = (-20.4022 + 0.4472*HR - 0.1263*W + 0.074*A)  / 4.184
static int get_session_calories(void) {
  int average_bpm = get_average_bpm();

  if (average_bpm <= 0 || s_elapsed_seconds <= 0) {
    return 0;
  }

  // Tous les coefficients sont multiplies par 10000 pour rester en entiers.
  int64_t numerator_scaled;

  if (s_profile.is_male) {
    numerator_scaled =
        (int64_t) -550969
        + (int64_t) 6309 * average_bpm
        + (int64_t) 1988 * s_profile.weight_kg
        + (int64_t) 2017 * s_profile.age_years;
  } else {
    numerator_scaled =
        (int64_t) -204022
        + (int64_t) 4472 * average_bpm
        - (int64_t) 1263 * s_profile.weight_kg
        + (int64_t) 740 * s_profile.age_years;
  }

  if (numerator_scaled < 0) {
    // Formule non applicable (effort trop faible / FC trop basse) : on
    // affiche 0 plutot qu'une valeur negative denuee de sens.
    return 0;
  }

  // kcal = numerator_scaled * elapsed_seconds / (10000 * 4.184 * 60)
  //      = numerator_scaled * elapsed_seconds / 2510400
  int64_t total_kcal =
      (numerator_scaled * (int64_t) s_elapsed_seconds) / 2510400;

  return (int) total_kcal;
}

// Frequence cardiaque maximale estimee, en entiers uniquement.
// Hommes : Tanaka   FCmax = 208 - 0,7 x age
// Femmes : Gulati   FCmax = 206 - 0,88 x age
static int get_max_heart_rate(void) {
  if (s_profile.is_male) {
    return 208 - (7 * s_profile.age_years + 5) / 10;
  }

  return 206 - (88 * s_profile.age_years + 50) / 100;
}

// 5 zones d'effort, en pourcentage de la FC maximale :
//   Zone 1 : moins de 60 %  (tres leger)              -> bleu
//   Zone 2 : 60 a 70 %      (endurance de base)       -> vert
//   Zone 3 : 70 a 80 %      (aerobie / tempo)         -> jaune
//   Zone 4 : 80 a 90 %      (intense / seuil)         -> orange
//   Zone 5 : 90 % et plus   (maximal)                 -> rouge pastel
// Hysteresis : la couleur ne change qu'apres avoir depasse le seuil de
// quelques bpm, pour eviter les clignotements autour d'une limite de zone.
#define HEART_RATE_ZONE_HYSTERESIS_BPM 3

static int s_heart_rate_zone = -1;

static int get_zone_upper_limit_bpm(int zone, int max_bpm) {
  static const int percents[4] = {60, 70, 80, 90};
  return (max_bpm * percents[zone]) / 100;
}

// Zone brute (0 a 4) sans hysteresis, pour les statistiques de la seance.
static int get_raw_heart_rate_zone(int bpm) {
  int max_bpm = get_max_heart_rate();
  int zone = 0;

  while (zone < 4 && bpm >= get_zone_upper_limit_bpm(zone, max_bpm)) {
    zone++;
  }

  return zone;
}

static int get_heart_rate_zone(int bpm) {
  int max_bpm = get_max_heart_rate();
  int zone = s_heart_rate_zone;

  if (zone < 0) {
    zone = 0;
    while (zone < 4 && bpm >= get_zone_upper_limit_bpm(zone, max_bpm)) {
      zone++;
    }
  } else {
    while (zone < 4 &&
           bpm >= get_zone_upper_limit_bpm(zone, max_bpm) +
                  HEART_RATE_ZONE_HYSTERESIS_BPM) {
      zone++;
    }

    while (zone > 0 &&
           bpm < get_zone_upper_limit_bpm(zone - 1, max_bpm) -
                 HEART_RATE_ZONE_HYSTERESIS_BPM) {
      zone--;
    }
  }

  s_heart_rate_zone = zone;
  return zone;
}

static GColor get_heart_rate_color(int bpm) {
  static const GColor zone_colors[5] = {
    GColorPictonBlue,
    GColorBrightGreen,
    ACCENT_YELLOW,
    GColorRajah,
    GColorLavenderIndigo
  };

  if (!s_heart_rate_available) {
    s_heart_rate_zone = -1;
    return GColorLightGray;
  }

  return zone_colors[get_heart_rate_zone(bpm)];
}

static void get_metric_text(void) {
  int minutes = s_elapsed_seconds / 60;
  int seconds = s_elapsed_seconds % 60;
  int distance_meters = get_distance_meters();

  // Par defaut, pas d'unite (cas DUREE, PAS, allure/vitesse indisponibles).
  s_metric_unit_text[0] = '\0';

  switch (s_current_metric) {
    case METRIC_DURATION:
      snprintf(
          s_metric_value_text,
          sizeof(s_metric_value_text),
          "%02d:%02d",
          minutes,
          seconds);

      snprintf(
          s_metric_label_text,
          sizeof(s_metric_label_text),
          "%s",
          tr(STR_LABEL_DURATION));
      break;

    case METRIC_STEPS:
      // Pas d'unite affichee : le libelle "PAS" suffit deja.
      snprintf(
          s_metric_value_text,
          sizeof(s_metric_value_text),
          "%d",
          s_current_steps);

      snprintf(
          s_metric_label_text,
          sizeof(s_metric_label_text),
          "%s",
          tr(STR_LABEL_STEPS));
      break;

    case METRIC_DISTANCE: {
      int whole_km = distance_meters / 1000;
      int decimal_hundredths = (distance_meters % 1000) / 10;

      snprintf(
          s_metric_value_text,
          sizeof(s_metric_value_text),
          "%d.%02d",
          whole_km,
          decimal_hundredths);

      snprintf(
          s_metric_unit_text,
          sizeof(s_metric_unit_text),
          "km");

      snprintf(
          s_metric_label_text,
          sizeof(s_metric_label_text),
          "%s",
          tr(STR_LABEL_DISTANCE));
      break;
    }

    case METRIC_HEART_RATE:
      if (s_heart_rate_available) {
        snprintf(
            s_metric_value_text,
            sizeof(s_metric_value_text),
            "%d",
            s_current_bpm);

        snprintf(
            s_metric_unit_text,
            sizeof(s_metric_unit_text),
            "bpm");
      } else {
        snprintf(
            s_metric_value_text,
            sizeof(s_metric_value_text),
            "%s",
            tr(STR_SENSOR_UNAVAILABLE));
      }

      snprintf(
          s_metric_label_text,
          sizeof(s_metric_label_text),
          "%s",
          tr(STR_LABEL_HEART_RATE));
      break;

    case METRIC_PACE:
      if (distance_meters > 10) {
        // Allure en secondes par kilometre, calculee en entiers uniquement.
        long pace_seconds_per_km =
            ((long) s_elapsed_seconds * 1000) / distance_meters;

        int pace_minutes = (int) (pace_seconds_per_km / 60);
        int pace_remaining_seconds = (int) (pace_seconds_per_km % 60);

        snprintf(
            s_metric_value_text,
            sizeof(s_metric_value_text),
            "%02d:%02d",
            pace_minutes,
            pace_remaining_seconds);

        snprintf(
            s_metric_unit_text,
            sizeof(s_metric_unit_text),
            "/km");
      } else {
        snprintf(
            s_metric_value_text,
            sizeof(s_metric_value_text),
            "--:--");

        snprintf(
            s_metric_unit_text,
            sizeof(s_metric_unit_text),
            "/km");
      }

      snprintf(
          s_metric_label_text,
          sizeof(s_metric_label_text),
          "%s",
          tr(STR_LABEL_PACE));
      break;

    case METRIC_SPEED:
      if (s_elapsed_seconds > 0) {
        // Vitesse en centiemes de km/h, calculee en entiers uniquement.
        long speed_hundredths_kmh =
            ((long) distance_meters * 360) / s_elapsed_seconds;

        int speed_whole = (int) (speed_hundredths_kmh / 100);
        int speed_decimal = (int) (speed_hundredths_kmh % 100) / 10;

        snprintf(
            s_metric_value_text,
            sizeof(s_metric_value_text),
            "%d.%d",
            speed_whole,
            speed_decimal);
      } else {
        snprintf(
            s_metric_value_text,
            sizeof(s_metric_value_text),
            "--");
      }

      snprintf(
          s_metric_unit_text,
          sizeof(s_metric_unit_text),
          "km/h");

      snprintf(
          s_metric_label_text,
          sizeof(s_metric_label_text),
          "%s",
          tr(STR_LABEL_SPEED));
      break;

    default:
      break;
  }
}

static void workout_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int16_t side_bar_width = 34;
  const int16_t content_width = bounds.size.w - side_bar_width;
  const int16_t half_height = bounds.size.h / 2;

  update_clock_text();
  get_metric_text();

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  GColor heart_rate_color = get_heart_rate_color(s_current_bpm);

  graphics_context_set_fill_color(ctx, heart_rate_color);
  graphics_fill_rect(
      ctx,
      GRect(0, 0, content_width, half_height),
      0,
      GCornerNone);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(
      ctx,
      GRect(content_width, 0, side_bar_width, bounds.size.h),
      0,
      GCornerNone);

  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(
      ctx,
      GPoint(0, half_height),
      GPoint(content_width, half_height));

  graphics_context_set_text_color(ctx, GColorBlack);
  GSize clock_size = graphics_text_layout_get_content_size(
      s_clock_text,
      s_roboto_condensed_extrabold_font,
      GRect(0, 0, content_width, half_height),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter);
  int16_t clock_y = (half_height - clock_size.h) / 2 + WORKOUT_CLOCK_Y_ADJUST;
  if (clock_y < 0) {
    clock_y = 0;
  }
  graphics_draw_text(
      ctx,
      s_clock_text,
      s_roboto_condensed_extrabold_font,
      GRect(0, clock_y, content_width, clock_size.h),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);

  bool heart_rate_unavailable =
      (s_current_metric == METRIC_HEART_RATE && !s_heart_rate_available);
  bool has_unit = (s_metric_unit_text[0] != '\0');

  if (heart_rate_unavailable) {
    // Cas particulier : message texte au lieu d'une valeur numerique,
    // pas d'unite a afficher a cote.
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(
        ctx,
        s_metric_value_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(0, half_height + 25, content_width, 55),
        GTextOverflowModeWordWrap,
        GTextAlignmentCenter,
        NULL);
  } else if (has_unit) {
    // Valeur en grand + unite en petit, cote a cote sur la meme ligne,
    // pour eviter tout retour a la ligne qui chevaucherait le libelle.
    GSize value_size = graphics_text_layout_get_content_size(
        s_metric_value_text,
        fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
        GRect(0, 0, content_width, 55),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);

    int16_t unit_width = graphics_text_layout_get_content_size(
        s_metric_unit_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(0, 0, 80, 24),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft).w;

    int16_t block_width = value_size.w + 6 + unit_width;
    int16_t block_x = (content_width - block_width) / 2;
    if (block_x < 0) {
      block_x = 0;
    }

    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(
        ctx,
        s_metric_value_text,
        fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
        GRect(block_x, half_height + 25, value_size.w + 4, 55),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft,
        NULL);

    graphics_draw_text(
        ctx,
        s_metric_unit_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(block_x + value_size.w + 6, half_height + 46, 40, 24),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft,
        NULL);
  } else {
    // Pas d'unite pour cette metrique (duree, pas encore de distance...).
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(
        ctx,
        s_metric_value_text,
        fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
        GRect(0, half_height + 25, content_width, 55),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter,
        NULL);
  }

  graphics_context_set_text_color(ctx, SECONDARY_TEXT_COLOR);
  graphics_draw_text(
      ctx,
      s_metric_label_text,
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(0, half_height + 80, content_width, 24),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);

  // Bouton haut : pause ou reprise
  if (s_is_paused) {
    draw_play_icon(ctx, content_width + 11, 28, GColorWhite);
  } else {
    draw_pause_icon(ctx, content_width + 11, 28, GColorWhite);
  }

  // Bouton du milieu : ampoule en séance, stop en pause
  if (s_is_paused) {
    draw_stop_icon(ctx, content_width + 10, 107, GColorWhite);
  } else {
    draw_bulb_icon(ctx, content_width + 10, 105, GColorWhite, s_stay_awake);
  }

  // Bouton bas : metrique suivante. "MET" remplace par des points de
  // suspension, le reste de la mise en page est inchange.
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(
      ctx,
      "...",
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(content_width, 179, side_bar_width, 22),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);
}

static void workout_tick_handler(struct tm *tick_time,
                                 TimeUnits units_changed) {
  if (!s_is_paused) {
    s_elapsed_seconds++;
    refresh_heart_rate_if_needed();
    refresh_steps_if_needed();
  }

  if (s_workout_layer != NULL) {
    layer_mark_dirty(s_workout_layer);
  }
}

static void countdown_timer_handler(void *context) {
  s_countdown_value--;

  if (s_countdown_value > 0) {
    update_countdown_text();

    s_countdown_timer = app_timer_register(
        1000,
        countdown_timer_handler,
        NULL);

    return;
  }

  s_countdown_timer = NULL;
  s_elapsed_seconds = 0;
  s_current_metric = METRIC_DURATION;

  // Vibration courte pour signaler le debut effectif du workout,
  // juste apres la fin du compte a rebours.
  vibes_short_pulse();

  window_stack_pop(true);
  window_stack_push(s_workout_window, true);
}

static void start_countdown(void) {
  s_countdown_value = 3;

  window_stack_push(s_countdown_window, true);

  s_countdown_timer = app_timer_register(
      1000,
      countdown_timer_handler,
      NULL);
}

static void menu_up_click_handler(ClickRecognizerRef recognizer,
                                  void *context) {
  s_selected_activity =
      (s_selected_activity + ACTIVITY_COUNT - 1) % ACTIVITY_COUNT;

  layer_mark_dirty(s_menu_layer);
}

static void menu_down_click_handler(ClickRecognizerRef recognizer,
                                    void *context) {
  s_selected_activity =
      (s_selected_activity + 1) % ACTIVITY_COUNT;

  layer_mark_dirty(s_menu_layer);
}

static void menu_select_click_handler(ClickRecognizerRef recognizer,
                                      void *context) {
  start_countdown();
}

static void menu_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, menu_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, menu_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, menu_select_click_handler);
}

static void menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, GColorWhite);

  s_menu_layer = layer_create(bounds);
  layer_set_update_proc(s_menu_layer, menu_layer_update_proc);
  layer_add_child(window_layer, s_menu_layer);
}

static void menu_window_unload(Window *window) {
  layer_destroy(s_menu_layer);
  s_menu_layer = NULL;
}

static void countdown_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, GColorBlack);

  s_countdown_activity_layer =
      text_layer_create(GRect(0, 12, bounds.size.w, 28));

  text_layer_set_background_color(s_countdown_activity_layer, GColorClear);
  text_layer_set_text_color(s_countdown_activity_layer, ACCENT_YELLOW);
  text_layer_set_text(
      s_countdown_activity_layer,
      tr(STR_ACTIVITY_WALK + (s_selected_activity)));
  text_layer_set_text_alignment(
      s_countdown_activity_layer,
      GTextAlignmentCenter);
  text_layer_set_font(
      s_countdown_activity_layer,
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

  layer_add_child(
      window_layer,
      text_layer_get_layer(s_countdown_activity_layer));

  // Calque du chiffre, centre verticalement sur l'ecran. Ajuster la
  // position avec COUNTDOWN_Y_ADJUST (negatif = plus haut).
  int16_t number_y =
      (bounds.size.h - COUNTDOWN_LAYER_HEIGHT) / 2 + COUNTDOWN_Y_ADJUST;

  s_countdown_number_layer = text_layer_create(
      GRect(0, number_y, bounds.size.w, COUNTDOWN_LAYER_HEIGHT));

  text_layer_set_background_color(s_countdown_number_layer, GColorClear);
  text_layer_set_text_color(s_countdown_number_layer, GColorWhite);
  text_layer_set_text_alignment(
      s_countdown_number_layer,
      GTextAlignmentCenter);
  text_layer_set_font(
      s_countdown_number_layer,
      s_roboto_condensed_extrabold_countdown_font);

  layer_add_child(
      window_layer,
      text_layer_get_layer(s_countdown_number_layer));

  update_countdown_text();
}

static void countdown_window_unload(Window *window) {
  text_layer_destroy(s_countdown_number_layer);
  s_countdown_number_layer = NULL;

  text_layer_destroy(s_countdown_activity_layer);
  s_countdown_activity_layer = NULL;
}

static void workout_up_click_handler(ClickRecognizerRef recognizer,
                                     void *context) {
  s_is_paused = !s_is_paused;

  layer_mark_dirty(s_workout_layer);
}

static void workout_select_click_handler(ClickRecognizerRef recognizer,
                                         void *context) {
  if (s_is_paused) {
    window_stack_push(s_stop_confirm_window, true);
  }
}

static bool is_metric_available(WorkoutMetric metric) {
  if (s_selected_activity != ACTIVITY_TRAINING) {
    return true;
  }

  // Entrainement libre : seulement la duree et la frequence cardiaque.
  return metric == METRIC_DURATION || metric == METRIC_HEART_RATE;
}

static WorkoutMetric get_next_metric(WorkoutMetric current) {
  WorkoutMetric next = current;

  do {
    next = (WorkoutMetric) ((next + 1) % METRIC_COUNT);
  } while (!is_metric_available(next));

  return next;
}

static void workout_down_click_handler(ClickRecognizerRef recognizer,
                                       void *context) {
  s_current_metric = get_next_metric(s_current_metric);

  layer_mark_dirty(s_workout_layer);
}

static void workout_back_click_handler(ClickRecognizerRef recognizer,
                                       void *context) {
  // Volontairement vide : empeche toute sortie accidentelle pendant la seance.
  // La seule sortie prevue passe par la pause puis l'ecran de confirmation.
}

static void workout_back_long_click_handler(ClickRecognizerRef recognizer,
                                          void *context) {
  s_stay_awake = !s_stay_awake;
  light_enable(s_stay_awake);
  vibes_short_pulse();
}

static void workout_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, workout_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, workout_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, workout_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, workout_back_click_handler);
  window_long_click_subscribe(
      BUTTON_ID_SELECT,
      700,
      workout_back_long_click_handler,
      NULL);
}

static void workout_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, GColorWhite);

  s_is_paused = false;
  s_stay_awake = false;
  light_enable(false);
  s_current_bpm = 0;
  s_heart_rate_available = false;
  s_heart_rate_zone = -1;
  s_seconds_since_last_bpm_read = 0;
  s_current_steps = 0;
  s_seconds_since_last_steps_read = 0;

  HealthValue starting_steps = health_service_sum_today(HealthMetricStepCount);
  s_steps_at_start = (int) starting_steps;
  if (s_steps_at_start < 0) {
    s_steps_at_start = 0;
  }

  s_bpm_sum = 0;
  s_bpm_sample_count = 0;
  s_zone_points_sum = 0;
  s_effort_choice = 0;
  s_effort_rating = 0;
  s_bpm_max = 0;

  health_service_set_heart_rate_sample_period(1);

  s_workout_layer = layer_create(bounds);
  layer_set_update_proc(s_workout_layer, workout_layer_update_proc);
  layer_add_child(window_layer, s_workout_layer);

  tick_timer_service_subscribe(
      SECOND_UNIT,
      workout_tick_handler);
}

static void workout_window_unload(Window *window) {
  tick_timer_service_unsubscribe();

  health_service_set_heart_rate_sample_period(0);

  layer_destroy(s_workout_layer);
  s_workout_layer = NULL;
}

static void draw_arrow_icon(GContext *ctx, int16_t x, int16_t y,
                            GColor color, bool up) {
  GPoint points[3];

  if (up) {
    points[0] = GPoint(x + 7, y);
    points[1] = GPoint(x, y + 10);
    points[2] = GPoint(x + 14, y + 10);
  } else {
    points[0] = GPoint(x, y);
    points[1] = GPoint(x + 14, y);
    points[2] = GPoint(x + 7, y + 10);
  }

  GPathInfo info = {
    .num_points = 3,
    .points = points
  };

  GPath *path = gpath_create(&info);
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

// Ecran de fin de seance : ligne 0 = continuer, lignes 1 a 4 = effort ressenti.
#define EFFORT_ROW_COUNT 5
#define EFFORT_ICON_UP_CENTER_Y 44
#define EFFORT_ICON_MID_CENTER_Y 116
#define EFFORT_ICON_DOWN_CENTER_Y 188
#define EFFORT_ROW_HEIGHT 30
#define EFFORT_LIST_TOP 62
#define EFFORT_SEPARATOR_GAP 8

static void stop_confirm_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int16_t side_bar_width = 34;
  int16_t content_width = bounds.size.w - side_bar_width;
  GFont font_24 = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont font_18 = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  graphics_context_set_fill_color(ctx, ACCENT_YELLOW);
  graphics_fill_rect(
      ctx,
      GRect(0, 0, content_width, bounds.size.h),
      0,
      GCornerNone);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(
      ctx,
      GRect(content_width, 0, side_bar_width, bounds.size.h),
      0,
      GCornerNone);

  graphics_context_set_text_color(ctx, SECONDARY_TEXT_COLOR);
  graphics_draw_text(
      ctx,
      tr(STR_EFFORT_QUESTION),
      font_18,
      GRect(6, 8, content_width - 12, 50),
      GTextOverflowModeWordWrap,
      GTextAlignmentCenter,
      NULL);

  static const GColor effort_colors[4] = {
    GColorPictonBlue,
    GColorBrightGreen,
    GColorRajah,
    GColorLavenderIndigo
  };

  for (int i = 0; i < EFFORT_ROW_COUNT; i++) {
    int16_t y = EFFORT_LIST_TOP + i * EFFORT_ROW_HEIGHT +
                ((i > 0) ? EFFORT_SEPARATOR_GAP : 0);
    const char *label = (i == 0)
        ? tr(STR_KEEP_GOING)
        : tr(STR_EFFORT_RELAXED + (i - 1));
    bool selected = (i == s_effort_choice);

    GFont row_font = font_24;
    int16_t text_y = y - 3;
    GSize label_size = graphics_text_layout_get_content_size(
        label,
        font_24,
        GRect(0, 0, 400, 40),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (label_size.w > content_width - 20) {
      row_font = font_18;
      text_y = y + 1;
    }

    GColor text_color = GColorBlack;

    if (selected) {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(
          ctx,
          GRect(4, y, content_width - 8, EFFORT_ROW_HEIGHT - 2),
          4,
          GCornersAll);

      if (i == 0) {
        text_color = GColorWhite;
      } else {
        graphics_context_set_fill_color(ctx, effort_colors[i - 1]);
        graphics_fill_rect(
            ctx,
            GRect(6, y + 2, content_width - 12, EFFORT_ROW_HEIGHT - 6),
            3,
            GCornersAll);
      }
    }

    graphics_context_set_text_color(ctx, text_color);
    graphics_draw_text(
        ctx,
        label,
        row_font,
        GRect(6, text_y, content_width - 12, EFFORT_ROW_HEIGHT),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter,
        NULL);
  }

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(
      ctx,
      GPoint(20, EFFORT_LIST_TOP + EFFORT_ROW_HEIGHT + 3),
      GPoint(content_width - 20, EFFORT_LIST_TOP + EFFORT_ROW_HEIGHT + 3));

  draw_arrow_icon(
      ctx, content_width + 10, EFFORT_ICON_UP_CENTER_Y - 6,
      GColorWhite, true);
  draw_check_icon(
      ctx, content_width + 9, EFFORT_ICON_MID_CENTER_Y - 5, GColorWhite);
  draw_arrow_icon(
      ctx, content_width + 10, EFFORT_ICON_DOWN_CENTER_Y - 6,
      GColorWhite, false);
}

static void stop_confirm_up_click_handler(ClickRecognizerRef recognizer,
                                          void *context) {
  s_effort_choice =
      (s_effort_choice + EFFORT_ROW_COUNT - 1) % EFFORT_ROW_COUNT;
  layer_mark_dirty(s_stop_confirm_layer);
}

static void stop_confirm_down_click_handler(ClickRecognizerRef recognizer,
                                            void *context) {
  s_effort_choice = (s_effort_choice + 1) % EFFORT_ROW_COUNT;
  layer_mark_dirty(s_stop_confirm_layer);
}

static void stop_confirm_select_click_handler(ClickRecognizerRef recognizer,
                                              void *context) {
  if (s_effort_choice == 0) {
    s_is_paused = false;
    layer_mark_dirty(s_workout_layer);
    window_stack_pop(true);
    return;
  }

  s_effort_rating = s_effort_choice;
  tick_timer_service_unsubscribe();
  window_stack_pop(true);
  window_stack_pop(true);
  window_stack_push(s_summary_window, true);
}

static void stop_confirm_back_click_handler(ClickRecognizerRef recognizer,
                                            void *context) {
  s_is_paused = false;
  layer_mark_dirty(s_workout_layer);
  window_stack_pop(true);
}

static void stop_confirm_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, stop_confirm_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, stop_confirm_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, stop_confirm_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, stop_confirm_back_click_handler);
}

static void stop_confirm_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_effort_choice = 0;
  s_stop_confirm_layer = layer_create(bounds);
  layer_set_update_proc(
      s_stop_confirm_layer,
      stop_confirm_layer_update_proc);

  layer_add_child(window_layer, s_stop_confirm_layer);
}

static void stop_confirm_window_unload(Window *window) {
  layer_destroy(s_stop_confirm_layer);
  s_stop_confirm_layer = NULL;
}

// Couleur du texte secondaire du resume, adaptee au fond pour rester lisible.
static GColor s_summary_label_color;

static GColor get_label_color_for_background(GColor background) {
  if (gcolor_equal(background, GColorPictonBlue) ||
      gcolor_equal(background, GColorLavenderIndigo)) {
    return GColorBlack;
  }

  return SECONDARY_TEXT_COLOR;
}

static void draw_summary_line(GContext *ctx, int16_t y, const char *value,
                              const char *unit, const char *label,
                              GFont value_font) {
  GSize value_size = graphics_text_layout_get_content_size(
      value,
      value_font,
      GRect(0, 0, 400, 50),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(
      ctx,
      value,
      value_font,
      GRect(6, y, 188, 44),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft,
      NULL);

  // L'unite garde sa taille d'origine (Gothic 28 gras), a droite de la valeur.
  if (unit[0] != '\0') {
    graphics_draw_text(
        ctx,
        unit,
        fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
        GRect(6 + value_size.w + 6, y + SUMMARY_UNIT_Y_OFFSET, 110, 34),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft,
        NULL);
  }

  graphics_context_set_text_color(ctx, s_summary_label_color);
  graphics_draw_text(
      ctx,
      label,
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(6, y + 42, 188, 22),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft,
      NULL);
}

// Score d'effort de 10 a 50 : FC moyenne (zone x 10) et effort ressenti.
// Renvoie 0 si aucune donnee exploitable.
static int get_effort_score(void) {
  static const int rating_scores[5] = {0, 10, 25, 40, 50};
  int hr_score = 0;

  // Zone moyenne mesuree a chaque echantillon (x10), de 10 a 50.
  if (s_bpm_sample_count > 0) {
    hr_score = (int) ((s_zone_points_sum * 10) / s_bpm_sample_count);
  }

  int rating_score = rating_scores[s_effort_rating];

  if (hr_score > 0 && rating_score > 0) {
    return (hr_score + 2 * rating_score) / 3;
  }

  return (hr_score > 0) ? hr_score : rating_score;
}

static GColor get_effort_color(void) {
  int score = get_effort_score();

  if (score == 0) {
    return GColorLightGray;
  } else if (score < 18) {
    return GColorPictonBlue;
  } else if (score < 28) {
    return GColorBrightGreen;
  } else if (score < 38) {
    return ACCENT_YELLOW;
  } else if (score < 46) {
    return GColorRajah;
  }

  return GColorLavenderIndigo;
}

static void summary_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  GColor background = (s_elapsed_seconds < MINIMUM_SESSION_SECONDS)
      ? ACCENT_YELLOW
      : get_effort_color();
  s_summary_label_color = get_label_color_for_background(background);
  graphics_context_set_fill_color(ctx, background);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (s_elapsed_seconds < MINIMUM_SESSION_SECONDS) {
    int minutes = s_elapsed_seconds / 60;
    int seconds = s_elapsed_seconds % 60;
    char duration_text[8];

    snprintf(
        duration_text,
        sizeof(duration_text),
        "%02d:%02d",
        minutes,
        seconds);

    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(
        ctx,
        tr(STR_TOO_SHORT),
        fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
        GRect(6, 24, bounds.size.w - 12, 40),
        GTextOverflowModeWordWrap,
        GTextAlignmentCenter,
        NULL);

    graphics_draw_text(
        ctx,
        duration_text,
        s_roboto_condensed_extrabold_font,
        GRect(0, 74, bounds.size.w, 80),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter,
        NULL);

    graphics_context_set_text_color(ctx, SECONDARY_TEXT_COLOR);
    graphics_draw_text(
        ctx,
        tr(STR_MIN_REQUIRED),
        fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(6, 160, bounds.size.w - 12, 60),
        GTextOverflowModeWordWrap,
        GTextAlignmentCenter,
        NULL);


    return;
  }

  int minutes = s_elapsed_seconds / 60;
  int seconds = s_elapsed_seconds % 60;
  int distance_meters = get_distance_meters();

  char duration_text[8];
  char steps_text[16];
  char distance_text[16];
  char pace_text[16];
  char speed_text[16];
  char bpm_avg_text[24];
  char bpm_max_text[24];
  char calories_text[16];

  snprintf(duration_text, sizeof(duration_text), "%02d:%02d", minutes, seconds);
  snprintf(steps_text, sizeof(steps_text), "%d", s_current_steps);
  snprintf(
      distance_text,
      sizeof(distance_text),
      "%d.%02d",
      distance_meters / 1000,
      (distance_meters % 1000) / 10);

  if (distance_meters > 10) {
    long pace_seconds_per_km =
        ((long) s_elapsed_seconds * 1000) / distance_meters;

    int pace_minutes = (int) (pace_seconds_per_km / 60);
    int pace_remaining_seconds = (int) (pace_seconds_per_km % 60);

    snprintf(
        pace_text,
        sizeof(pace_text),
        "%02d:%02d",
        pace_minutes,
        pace_remaining_seconds);
  } else {
    snprintf(pace_text, sizeof(pace_text), "--:--");
  }

  if (s_elapsed_seconds > 0) {
    long speed_hundredths_kmh =
        ((long) distance_meters * 360) / s_elapsed_seconds;

    int speed_whole = (int) (speed_hundredths_kmh / 100);
    int speed_decimal = (int) (speed_hundredths_kmh % 100) / 10;

    snprintf(speed_text, sizeof(speed_text), "%d.%d", speed_whole, speed_decimal);
  } else {
    snprintf(speed_text, sizeof(speed_text), "--");
  }

  int average_bpm = get_average_bpm();

  if (average_bpm > 0) {
    snprintf(bpm_avg_text, sizeof(bpm_avg_text), "%d", average_bpm);
  } else {
    snprintf(bpm_avg_text, sizeof(bpm_avg_text), "%s", tr(STR_SENSOR_UNAVAILABLE));
  }

  if (s_bpm_max > 0) {
    snprintf(bpm_max_text, sizeof(bpm_max_text), "%d", s_bpm_max);
  } else {
    snprintf(bpm_max_text, sizeof(bpm_max_text), "%s", tr(STR_SENSOR_UNAVAILABLE));
  }

  int calories = get_session_calories();
  snprintf(calories_text, sizeof(calories_text), "%d", calories);

  // Entrainement : on masque les valeurs non mesurees.
  bool hide_unmeasured = (s_selected_activity == ACTIVITY_TRAINING);

  GRect scroll_frame = GRect(0, -s_summary_scroll_offset, bounds.size.w, 600);

  // Titre : police personnalisee, avec repli sur Gothic 28 gras si le nom
  // de l'activite est trop large pour l'ecran.
  const char *title = tr(STR_ACTIVITY_WALK + (s_selected_activity));
  GFont title_font = s_summary_font;
  GSize title_size = graphics_text_layout_get_content_size(
      title,
      title_font,
      GRect(0, 0, 400, 50),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  if (title_size.w > bounds.size.w - 12) {
    title_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  }

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(
      ctx,
      title,
      title_font,
      GRect(6, scroll_frame.origin.y + 4, bounds.size.w - 12, 44),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft,
      NULL);

  graphics_context_set_text_color(ctx, s_summary_label_color);
  graphics_draw_text(
      ctx,
      tr(STR_SESSION_DONE),
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(6, scroll_frame.origin.y + 46, bounds.size.w - 12, 28),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft,
      NULL);

  GFont value_font = s_summary_font;
  GFont message_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont bpm_font_avg = (average_bpm > 0) ? value_font : message_font;
  GFont bpm_font_max = (s_bpm_max > 0) ? value_font : message_font;
  const char *bpm_avg_unit = (average_bpm > 0) ? "bpm" : "";
  const char *bpm_max_unit = (s_bpm_max > 0) ? "bpm" : "";

  int16_t y = scroll_frame.origin.y + 84;
  int16_t step = 72;

  if (s_effort_rating > 0) {
    const char *effort_text = tr(STR_EFFORT_RELAXED + (s_effort_rating - 1));
    GFont effort_font = value_font;
    GSize effort_size = graphics_text_layout_get_content_size(
        effort_text,
        value_font,
        GRect(0, 0, 400, 50),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);

    if (effort_size.w > bounds.size.w - 12) {
      effort_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    }

    draw_summary_line(
        ctx, y, effort_text, "", tr(STR_SUMMARY_EFFORT), effort_font);
    y += step;
  }

#if EFFORT_DEBUG
  {
    char debug_text[40];
    int debug_zone_x10 = (s_bpm_sample_count > 0)
        ? (int) ((s_zone_points_sum * 10) / s_bpm_sample_count)
        : 0;

    snprintf(
        debug_text,
        sizeof(debug_text),
        "%d/%d/%d/%d",
        get_effort_score(),
        debug_zone_x10,
        average_bpm,
        s_effort_rating);
    draw_summary_line(
        ctx, y, debug_text, "", "DEBUG SCORE/ZONE/BPM/REP",
        fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    y += step;
  }
#endif

  draw_summary_line(ctx, y, duration_text, "", tr(STR_LABEL_DURATION), value_font);
  y += step;

  if (s_selected_activity != ACTIVITY_TRAINING) {
    draw_summary_line(ctx, y, steps_text, tr(STR_UNIT_STEPS), tr(STR_LABEL_STEPS), value_font);
    y += step;

    draw_summary_line(ctx, y, distance_text, "km", tr(STR_LABEL_DISTANCE), value_font);
    y += step;

    draw_summary_line(ctx, y, pace_text, "/km", tr(STR_SUMMARY_PACE), value_font);
    y += step;

    draw_summary_line(ctx, y, speed_text, "km/h", tr(STR_SUMMARY_SPEED), value_font);
    y += step;
  }

  if (!(hide_unmeasured && average_bpm <= 0)) {
    draw_summary_line(
        ctx, y, bpm_avg_text, bpm_avg_unit, tr(STR_SUMMARY_HR_AVG), bpm_font_avg);
    y += step;
  }

  if (!(hide_unmeasured && s_bpm_max <= 0)) {
    draw_summary_line(
        ctx, y, bpm_max_text, bpm_max_unit, tr(STR_SUMMARY_HR_MAX), bpm_font_max);
    y += step;
  }

  if (!(hide_unmeasured && calories <= 0)) {
    draw_summary_line(ctx, y, calories_text, "kcal", tr(STR_SUMMARY_CALORIES), value_font);
    y += step;
  }

  s_summary_content_height = (y + 12) - scroll_frame.origin.y;
}

static void summary_up_click_handler(ClickRecognizerRef recognizer,
                                     void *context) {
  s_summary_scroll_offset -= SUMMARY_SCROLL_STEP;

  if (s_summary_scroll_offset < 0) {
    s_summary_scroll_offset = 0;
  }

  layer_mark_dirty(s_summary_layer);
}

static void summary_down_click_handler(ClickRecognizerRef recognizer,
                                       void *context) {
  GRect bounds = layer_get_bounds(s_summary_layer);
  int max_offset = s_summary_content_height - bounds.size.h;

  if (max_offset < 0) {
    max_offset = 0;
  }

  s_summary_scroll_offset += SUMMARY_SCROLL_STEP;

  if (s_summary_scroll_offset > max_offset) {
    s_summary_scroll_offset = max_offset;
  }

  layer_mark_dirty(s_summary_layer);
}

static void summary_select_click_handler(ClickRecognizerRef recognizer,
                                         void *context) {
  // Le menu est la premiere fenetre poussee dans init() et reste toujours
  // au fond de la pile : on depile donc tout le reste pour y revenir,
  // sans jamais fermer l'application.
  while (window_stack_get_top_window() != s_menu_window) {
    window_stack_pop(true);
  }
}

static void summary_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, summary_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, summary_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, summary_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, summary_select_click_handler);
}

static void summary_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, ACCENT_YELLOW);

  s_summary_scroll_offset = 0;

  s_summary_layer = layer_create(bounds);
  layer_set_update_proc(s_summary_layer, summary_layer_update_proc);
  layer_add_child(window_layer, s_summary_layer);
}

static void summary_window_unload(Window *window) {
  layer_destroy(s_summary_layer);
  s_summary_layer = NULL;
}

static void profile_sanitize(void) {
  s_profile.is_male = s_profile.is_male ? 1 : 0;

  if (s_profile.age_years < 10) s_profile.age_years = 10;
  if (s_profile.age_years > 100) s_profile.age_years = 100;

  if (s_profile.height_cm < 120) s_profile.height_cm = 120;
  if (s_profile.height_cm > 230) s_profile.height_cm = 230;

  if (s_profile.weight_kg < 30) s_profile.weight_kg = 30;
  if (s_profile.weight_kg > 250) s_profile.weight_kg = 250;
}

static void profile_load(void) {
  if (persist_exists(PROFILE_PERSIST_KEY)) {
    persist_read_data(PROFILE_PERSIST_KEY, &s_profile, sizeof(s_profile));
  }

  profile_sanitize();
}

static void profile_save(void) {
  persist_write_data(PROFILE_PERSIST_KEY, &s_profile, sizeof(s_profile));
}

// Clay envoie les curseurs sous forme de nombres et le choix du sexe
// sous forme de texte : on accepte les deux.
static int tuple_to_int(const Tuple *tuple) {
  if (tuple->type == TUPLE_CSTRING) {
    return atoi(tuple->value->cstring);
  }

  return (int) tuple->value->int32;
}

static void profile_inbox_received_handler(DictionaryIterator *iter,
                                           void *context) {
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_Sex);
  if (tuple) {
    s_profile.is_male = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_Age);
  if (tuple) {
    s_profile.age_years = (int16_t) tuple_to_int(tuple);
  }

  tuple = dict_find(iter, MESSAGE_KEY_Height);
  if (tuple) {
    s_profile.height_cm = (int16_t) tuple_to_int(tuple);
  }

  tuple = dict_find(iter, MESSAGE_KEY_Weight);
  if (tuple) {
    s_profile.weight_kg = (int16_t) tuple_to_int(tuple);
  }

  profile_sanitize();
  profile_save();

}

static void init(void) {
  profile_load();
  app_message_register_inbox_received(profile_inbox_received_handler);
  app_message_open(128, 128);

  s_roboto_condensed_extrabold_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_EXTRABOLD_56));

  s_roboto_condensed_extrabold_countdown_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_EXTRABOLD_100));

  s_summary_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_EXTRABOLD_34));

  s_emoji_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_FONT_NOTO_EMOJI_40));

  s_menu_window = window_create();
  window_set_click_config_provider(
      s_menu_window,
      menu_click_config_provider);
  window_set_window_handlers(
      s_menu_window,
      (WindowHandlers) {
          .load = menu_window_load,
          .unload = menu_window_unload,
      });

  s_countdown_window = window_create();
  window_set_window_handlers(
      s_countdown_window,
      (WindowHandlers) {
          .load = countdown_window_load,
          .unload = countdown_window_unload,
      });

  s_workout_window = window_create();
  window_set_click_config_provider(
      s_workout_window,
      workout_click_config_provider);
  window_set_window_handlers(
      s_workout_window,
      (WindowHandlers) {
          .load = workout_window_load,
          .unload = workout_window_unload,
      });

  s_stop_confirm_window = window_create();
  window_set_click_config_provider(
      s_stop_confirm_window,
      stop_confirm_click_config_provider);
  window_set_window_handlers(
      s_stop_confirm_window,
      (WindowHandlers) {
          .load = stop_confirm_window_load,
          .unload = stop_confirm_window_unload,
      });

  s_summary_window = window_create();
  window_set_click_config_provider(
      s_summary_window,
      summary_click_config_provider);
  window_set_window_handlers(
      s_summary_window,
      (WindowHandlers) {
          .load = summary_window_load,
          .unload = summary_window_unload,
      });

  window_stack_push(s_menu_window, true);
}

static void deinit(void) {
  if (s_countdown_timer != NULL) {
    app_timer_cancel(s_countdown_timer);
    s_countdown_timer = NULL;
  }

  tick_timer_service_unsubscribe();

  window_destroy(s_summary_window);
  window_destroy(s_stop_confirm_window);
  window_destroy(s_workout_window);
  window_destroy(s_countdown_window);
  window_destroy(s_menu_window);

  fonts_unload_custom_font(s_roboto_condensed_extrabold_font);
  fonts_unload_custom_font(s_roboto_condensed_extrabold_countdown_font);
  fonts_unload_custom_font(s_summary_font);
  fonts_unload_custom_font(s_emoji_font);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
