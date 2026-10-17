#include <pebble.h>
#include <stdlib.h>
#include <string.h>
#include "i18n.h"
#include "steps.h"
#include "achievements.h"
#include "journal.h"
#include "journal_persist.h"
#include "zones.h"

static int get_effort_score(void);
static void timeline_format_clock(time_t timestamp,
                                  char *buffer, size_t size);
static void detail_cancel_scroll_animation(void);
static void session_return_to_menu(void);


typedef enum {
  ACTIVITY_WALK = 0,
  ACTIVITY_RUN,
  ACTIVITY_TRAINING,
  ACTIVITY_COUNT
} Activity;

#define MENU_ROWS (ACTIVITY_COUNT + 1)

typedef enum {
  METRIC_DURATION = 0,
  METRIC_STEPS,
  METRIC_DISTANCE,
  METRIC_HEART_RATE,
  METRIC_HEART_RATE_GRAPH,
  METRIC_PACE,
  METRIC_PACE_GRAPH,
  METRIC_SPEED,
  METRIC_COUNT
} WorkoutMetric;

#define MINIMUM_SESSION_SECONDS 120
// 1 = ligne de debug du score d'effort dans le resume (ne jamais commiter a 1).
#define EFFORT_DEBUG 0
#define SUMMARY_SCROLL_STEP 72
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
// 1 = la zone moyenne du score d'effort utilise la mesure affichee (brute
// si valide), sans le retard du filtre systeme. 0 = mesure filtree.
#define EFFORT_ZONE_USES_DISPLAY_BPM 1

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

// Palette des 5 niveaux (zones cardiaques, effort, fond du resume).
// Teintes franches : les pastel (Rajah, LavenderIndigo) se lisaient mal sur
// l'ecran reel de la montre. Le texte est choisi selon le fond, voir
// get_text_color_for_background() et get_label_color_for_background().
#define COLOR_LEVEL_1 GColorPictonBlue
#define COLOR_LEVEL_2 GColorBrightGreen
#define COLOR_LEVEL_3 ACCENT_YELLOW
#define COLOR_LEVEL_4 GColorOrange
#define COLOR_LEVEL_5 GColorVividViolet

// Reglage du chiffre du compte a rebours (ecran Emery 200x228)
#define COUNTDOWN_LAYER_HEIGHT 130
#define COUNTDOWN_Y_ADJUST 0
#define WORKOUT_CLOCK_Y_ADJUST (-8)
#define SUMMARY_UNIT_Y_OFFSET 6
// Largeur de la barre noire des boutons (seance et fin de seance).
#define SIDE_BAR_WIDTH 34

// Profil utilisateur, regle depuis le telephone (page Clay) et memorise
// sur la montre.
typedef struct {
  int16_t is_male;     // 1 = homme, 0 = femme
  int16_t age_years;
  int16_t height_cm;
  int16_t weight_kg;
  int16_t resting_hr;  // 0 = non regle (pourcentage du maximum), sinon Karvonen
} UserProfile;

static UserProfile s_profile = {
  .is_male = DEFAULT_USER_IS_MALE,
  .age_years = DEFAULT_USER_AGE_YEARS,
  .height_cm = DEFAULT_USER_HEIGHT_CM,
  .weight_kg = DEFAULT_USER_WEIGHT_KG
};

// Pas personnalise, regle sur le telephone (page Clay, section "Step length").
// Les bornes absolues (cm) doivent rester identiques a celles de config.js.
// Les bornes relatives (pourcentage de la taille) ne sont verifiees qu'ici,
// car la page Clay ne connait pas la taille au moment du reglage.
#define STRIDE_PERSIST_KEY 3
#define STRIDE_WALK_MIN_CM 30
#define STRIDE_WALK_MAX_CM 100
#define STRIDE_RUN_MIN_CM 40
#define STRIDE_RUN_MAX_CM 160
#define STRIDE_WALK_MIN_PERCENT 25
#define STRIDE_WALK_MAX_PERCENT 70
#define STRIDE_RUN_MIN_PERCENT 30
#define STRIDE_RUN_MAX_PERCENT 100
// Pas estime a partir de la taille, en milliemes : marche 0,415 (valeur
// usuelle), course 0,55 (provisoire, a ajuster selon les retours du terrain).
#define STRIDE_WALK_DEFAULT_PERMILLE 415
#define STRIDE_RUN_DEFAULT_PERMILLE 550

typedef struct {
  int16_t units_imperial;  // 0 = metrique, 1 = imperial (affichage)
  int16_t walk_enabled;    // 0 = pas de marche estime a partir de la taille
  int16_t walk_cm;
  int16_t run_enabled;     // 0 = pas de course estime a partir de la taille
  int16_t run_cm;
} StrideSettings;

static StrideSettings s_stride = {
  .units_imperial = 0,
  .walk_enabled = 0,
  .walk_cm = 0,
  .run_enabled = 0,
  .run_cm = 0
};

// Horloge : format 12 h et AM/PM, regles sur le telephone (cle 4).
// OFFSET : decalage du AM/PM par rapport au coin bas droit de l'heure.
#define CLOCK_PERSIST_KEY 4
#define CLOCK_AMPM_OFFSET_X 18
#define CLOCK_AMPM_OFFSET_Y 24

typedef struct {
  int16_t use_12h;    // 0 = 24 h, 1 = 12 h
  int16_t show_ampm;  // 1 = afficher AM/PM (12 h seulement)
  int16_t hide_hour_zero;  // 1 = pas de zero devant l'heure (24 h)
} ClockSettings;

static ClockSettings s_clock = { .use_12h = 0, .show_ampm = 1 };
static GFont s_clock_font = NULL;

// Alertes (vibration et son), reglees sur le telephone (cle 5).
#define ALERT_PERSIST_KEY 5
#define ALERT_ZONE_MIN_GAP_SECONDS 20

typedef struct {
  int16_t vibrate_km;    // 1 = vibrer a chaque km ou mile
  int16_t vibrate_zone;  // 1 = vibrer au changement de zone de FC
  int16_t sound_km;      // 1 = bip a chaque km ou mile
  int16_t sound_zone;    // 1 = bip au changement de zone de FC
  int16_t interval_minutes;  // 0 = desactive, sinon 2, 5, 10, 15, 20, 30 ou 60
  int16_t vibrate_interval;  // 1 = vibrer a chaque intervalle
  int16_t sound_interval;    // 1 = bip a chaque intervalle
  int16_t vibration_level;   // 0 = discrete, 1 = normale, 2 = renforcee
} AlertSettings;

static AlertSettings s_alerts = { 0, 0, 0, 0, 0, 0, 0, 1 };
static int s_alert_km_done = 0;
static int s_alert_interval_done = 0;
static int s_alert_zone = -1;
static int s_alert_zone_second = 0;

static GFont s_roboto_condensed_extrabold_font = NULL;
static GFont s_roboto_condensed_extrabold_countdown_font = NULL;
static GFont s_summary_font = NULL;
static GFont s_emoji_font = NULL;

static Window *s_menu_window;
static Window *s_countdown_window;
static Window *s_workout_window;
static Window *s_journal_window;
static Layer *s_journal_layer;
static int s_menu_selection = 0;
static int s_journal_selection = 0;
static int s_journal_first_visible = 0;
static int s_journal_count = 0;
static JournalSession s_journal_list[JOURNAL_SLOTS];
static Window *s_detail_window;
static Layer *s_detail_layer;
static JournalSession s_detail_session;
static int s_detail_scroll_offset = 0;
static int s_detail_content_height = 0;

// Defilement court du resume : 6 frames de 25 ms.
#define DETAIL_SCROLL_FRAMES 6
#define DETAIL_SCROLL_FRAME_MS 25
static AppTimer *s_detail_scroll_timer = NULL;
static int s_detail_scroll_frame = 0;
static int s_detail_scroll_from = 0;
static int s_detail_scroll_to = 0;

static Window *s_timeline_window;
static Layer *s_timeline_layer;
// Selection = numero de page, et non numero d'evenement.
static int s_timeline_selection = 0;
#define TIMELINE_PAGE_TOP 36
#define TIMELINE_PAGE_BOTTOM 8
#define TIMELINE_EVENT_GAP 6
#define TIMELINE_PAGE_SPACING 24
#define TIMELINE_KICK_PX 2
static uint8_t s_timeline_page_starts[JOURNAL_MAX_EVENTS + 4];
static int s_timeline_page_count = 0;
#define TIMELINE_BLOCK_HEIGHT 96
#define TIMELINE_ZONE_HEIGHT 84
#define TIMELINE_ZONE_EXTRA_HEIGHT \
    (TIMELINE_ZONE_HEIGHT - TIMELINE_BLOCK_HEIGHT)
#define TIMELINE_SCROLL_FRAMES 14
#define TIMELINE_SCROLL_FRAME_MS 20
static AppTimer *s_timeline_scroll_timer = NULL;
static int s_timeline_scroll_offset = 0;
static int s_timeline_scroll_from = 0;
static int s_timeline_scroll_target = 0;
static int s_timeline_scroll_frame = 0;
static int s_timeline_stretch = 0;
// 1 lorsque le resume final a ete ouvert depuis la frise : Retour y revient.
static bool s_summary_from_timeline = false;
static Window *s_stop_confirm_window;
static Window *s_summary_window;
static Window *s_save_window;
static Layer *s_save_layer;
static bool s_save_full = false;
static bool s_save_error = false;
static bool s_session_pending_save = false;

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
static int s_paused_seconds = 0;
static bool s_stay_awake = false;

static AppTimer *s_countdown_timer;

static char s_countdown_text[4];
static char s_clock_text[16];
static char s_clock_suffix_text[8];
static char s_metric_value_text[24];
static char s_metric_unit_text[12];
static char s_metric_label_text[24];

static int s_current_bpm = 0;
// Premiere FC valide obtenue pendant la seance, conservee en RAM.
static int s_session_start_bpm = 0;
static bool s_heart_rate_available = false;
static int s_seconds_since_last_bpm_read = 0;

static int s_current_steps = 0;
static StepCounter s_step_counter;
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

// Suivi de la seance pour le journal (voir journal.h).
static JournalSession s_journal;
// Duree consecutive actuelle ; le Journal conserve le maximum.
static uint16_t s_journal_redline_current = 0;
// Durees des passages : memoire seulement, indexees par evenement.
static uint16_t s_journal_zone_duration[JOURNAL_MAX_EVENTS];
static uint16_t s_journal_zone_pass_seconds = 0;
static int s_journal_zone_pass_index = -1;
static JournalGoals s_goals;
static time_t s_session_start_time = 0;
static time_t s_session_end_time = 0;
// Duree totale provisoire, capturee a chaque appui sur Stop.
static uint32_t s_session_total_at_stop = 0;
// Unite des bornes, fixee au debut de la seance pour la frise.
static bool s_session_units_imperial = false;
// Plages nominales capturees au debut de seance.
static int s_session_zone_limits[4];
static int s_session_start_hour = 0;
static int s_session_start_day = 0;
static int s_journal_zone_event = -1;
static int s_journal_zone_event_second = 0;
static int s_journal_zone_events = 0;
static int s_journal_borne_done = 0;
static int s_journal_borne_events = 0;
static int s_journal_flame_done = 0;
static int s_journal_flame_events = 0;
static int s_zone_run_seconds = 0;  // duree consecutive en zone 4 ou 5

// Defilement vertical de l'ecran de resume
static int s_summary_scroll_offset = 0;
static int s_summary_content_height = 0;

// Couleur du texte lisible sur un fond de la palette : blanc sur le violet
// fonce, noir partout ailleurs.
static GColor get_text_color_for_background(GColor background) {
  if (gcolor_equal(background, COLOR_LEVEL_5)) {
    return GColorWhite;
  }

  return GColorBlack;
}

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
  const int16_t row_height = 54;
  const int16_t first_row_y = 6;

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  for (int i = 0; i < MENU_ROWS; i++) {
    int16_t row_y = first_row_y + i * row_height;
    bool selected = (i == s_menu_selection);

    if (selected) {
      graphics_context_set_fill_color(ctx, ACCENT_YELLOW);
      graphics_fill_rect(
          ctx,
          GRect(0, row_y, bounds.size.w, row_height),
          0,
          GCornerNone);
    }

    GColor icon_color = selected ? GColorBlack : GColorDarkGray;

    if (i < ACTIVITY_COUNT) {
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
    } else {
      graphics_context_set_text_color(ctx, icon_color);
      graphics_draw_text(
          ctx,
          "\xF0\x9F\x93\x96",
          s_emoji_font,
          GRect(8, row_y + 3, 60, 50),
          GTextOverflowModeTrailingEllipsis,
          GTextAlignmentCenter,
          NULL);

      graphics_context_set_text_color(ctx, GColorBlack);
      graphics_draw_text(
          ctx,
          "Journal",
          fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
          GRect(76, row_y + 14, bounds.size.w - 82, 32),
          GTextOverflowModeTrailingEllipsis,
          GTextAlignmentLeft,
          NULL);
    }
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

  int hour = current_time->tm_hour;
  int minute = current_time->tm_min;

  s_clock_suffix_text[0] = '\0';

  if (s_clock.use_12h) {
    if (s_clock.show_ampm) {
      snprintf(s_clock_suffix_text, sizeof(s_clock_suffix_text),
               "%s", (hour < 12) ? "AM" : "PM");
    }
    hour = hour % 12;
    if (hour == 0) {
      hour = 12;
    }
    snprintf(s_clock_text, sizeof(s_clock_text), "%d:%02d", hour, minute);
  } else {
    if (s_clock.hide_hour_zero) {
      snprintf(s_clock_text, sizeof(s_clock_text), "%d:%02d", hour, minute);
    } else {
      snprintf(s_clock_text, sizeof(s_clock_text), "%02d:%02d", hour, minute);
    }
  }
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

  if (s_session_start_bpm == 0 &&
      display_bpm >= 30 && display_bpm <= 230) {
    s_session_start_bpm = display_bpm;
  }

  if (filtered_bpm > 0) {
    s_bpm_sum += filtered_bpm;
    s_bpm_sample_count++;
    s_zone_points_sum += get_raw_heart_rate_zone(
        EFFORT_ZONE_USES_DISPLAY_BPM ? display_bpm : filtered_bpm) + 1;

    if (filtered_bpm > s_bpm_max) {
      s_bpm_max = filtered_bpm;
    }
  }
}

static void refresh_steps_now(void) {
  HealthValue total_today = health_service_sum_today(HealthMetricStepCount);
  s_current_steps = step_counter_update(&s_step_counter, (int) total_today);
}

static void refresh_steps_if_needed(void) {
  s_seconds_since_last_steps_read++;

  if (s_seconds_since_last_steps_read < STEPS_REFRESH_INTERVAL_SECONDS) {
    return;
  }

  s_seconds_since_last_steps_read = 0;
  refresh_steps_now();
}

static int get_default_stride_cm(Activity activity) {
  int permille = (activity == ACTIVITY_RUN)
      ? STRIDE_RUN_DEFAULT_PERMILLE
      : STRIDE_WALK_DEFAULT_PERMILLE;

  return s_profile.height_cm * permille / 1000;
}

// Une valeur personnalisee n'est retenue que si elle est dans les bornes
// absolues ET plausible par rapport a la taille actuelle.
static bool is_custom_stride_valid(int cm, int min_cm, int max_cm,
                                   int min_percent, int max_percent) {
  if (cm < min_cm || cm > max_cm) {
    return false;
  }

  return cm * 100 >= min_percent * s_profile.height_cm &&
         cm * 100 <= max_percent * s_profile.height_cm;
}

static int get_stride_length_cm_for(Activity activity) {
  int walk_cm = get_default_stride_cm(ACTIVITY_WALK);
  int run_cm = get_default_stride_cm(ACTIVITY_RUN);

  if (s_stride.walk_enabled &&
      is_custom_stride_valid(s_stride.walk_cm,
                             STRIDE_WALK_MIN_CM, STRIDE_WALK_MAX_CM,
                             STRIDE_WALK_MIN_PERCENT, STRIDE_WALK_MAX_PERCENT)) {
    walk_cm = s_stride.walk_cm;
  }

  if (s_stride.run_enabled &&
      is_custom_stride_valid(s_stride.run_cm,
                             STRIDE_RUN_MIN_CM, STRIDE_RUN_MAX_CM,
                             STRIDE_RUN_MIN_PERCENT, STRIDE_RUN_MAX_PERCENT)) {
    run_cm = s_stride.run_cm;
  }

  // Coherence : un pas de course n'est jamais plus court qu'un pas de marche.
  if (run_cm < walk_cm) {
    run_cm = walk_cm;
  }

  return (activity == ACTIVITY_RUN) ? run_cm : walk_cm;
}

static int get_stride_length_cm(void) {
  return get_stride_length_cm_for(s_selected_activity);
}

// Distance en metres, en entier : evite tout %f (non supporte par la
// libc embarquee du firmware Pebble, cause du bug "floating point").
static int get_distance_meters(void) {
  int stride_length_cm = get_stride_length_cm();
  return (s_current_steps * stride_length_cm) / 100;
}

// Unites d'affichage : tout est calcule en metrique, seules la conversion et
// les libelles changent en mode imperial. 1 mile = 1609,344 m.
static const char *get_distance_unit(void) {
  return s_stride.units_imperial ? "mi" : "km";
}

static const char *get_pace_unit(void) {
  return s_stride.units_imperial ? "/mi" : "/km";
}

static const char *get_speed_unit(void) {
  return s_stride.units_imperial ? "mph" : "km/h";
}

// Distance en centiemes de km ou de miles.
static int get_distance_hundredths(int distance_meters) {
  if (s_stride.units_imperial) {
    return (int) (((int64_t) distance_meters * 100000) / 1609344);
  }

  return distance_meters / 10;
}

// Allure en secondes par km ou par mile (distance_meters doit etre > 0).
static long get_pace_seconds(int distance_meters) {
  if (s_stride.units_imperial) {
    return (long) (((int64_t) s_elapsed_seconds * 1609344) /
                   ((int64_t) distance_meters * 1000));
  }

  return ((long) s_elapsed_seconds * 1000) / distance_meters;
}

// Allure affichable : distance suffisante et pas plus de 99:59.
#define PACE_MAX_SECONDS 5999

static bool is_pace_displayable(int distance_meters) {
  return distance_meters > 10 &&
         get_pace_seconds(distance_meters) <= PACE_MAX_SECONDS;
}

// Vitesse en centiemes de km/h ou de mph (s_elapsed_seconds doit etre > 0).
static long get_speed_hundredths(int distance_meters) {
  if (s_stride.units_imperial) {
    return (long) (((int64_t) distance_meters * 360000000) /
                   ((int64_t) 1609344 * s_elapsed_seconds));
  }

  return ((long) distance_meters * 360) / s_elapsed_seconds;
}

// Historique en RAM : uniquement des mesures reelles.
// 0 indique une mesure absente, jamais une allure nulle.
#define GRAPH_POINT_CAPACITY 256
#define GRAPH_SAMPLE_SECONDS 30

typedef struct {
  uint32_t active_seconds;
  uint16_t bpm;
  uint16_t pace_seconds_km;
} WorkoutGraphPoint;

static WorkoutGraphPoint s_graph_points[GRAPH_POINT_CAPACITY];
static int s_graph_count = 0;
static int s_graph_best_pace = 0;
static int s_graph_slowest_pace = 0;

// Trois positions consecutives permettent une difference sur 60 s.
static int64_t s_graph_distance_cm[3];
static int s_graph_distance_count = 0;

static void workout_graph_reset(void) {
  s_graph_count = 1;
  s_graph_points[0] = (WorkoutGraphPoint) {0, 0, 0};
  s_graph_best_pace = 0;
  s_graph_slowest_pace = 0;
  s_graph_distance_count = 1;
  s_graph_distance_cm[0] = 0;
}

// Sous-echantillonnage : conserve le premier point et le plus recent.
// Les horodatages restent ceux des vraies mesures.
static void workout_graph_compact(void) {
  int out = 0;
  for (int i = 0; i < s_graph_count; i += 2) {
    s_graph_points[out++] = s_graph_points[i];
  }
  if ((s_graph_count - 1) % 2 != 0) {
    s_graph_points[out++] = s_graph_points[s_graph_count - 1];
  }
  s_graph_count = out;
}

static void workout_graph_sample(void) {
  if (s_elapsed_seconds <= 0 ||
      s_elapsed_seconds % GRAPH_SAMPLE_SECONDS != 0) {
    return;
  }

  int64_t distance_cm =
      (int64_t) s_current_steps * get_stride_length_cm();

  if (s_graph_distance_count < 3) {
    s_graph_distance_cm[s_graph_distance_count++] = distance_cm;
  } else {
    s_graph_distance_cm[0] = s_graph_distance_cm[1];
    s_graph_distance_cm[1] = s_graph_distance_cm[2];
    s_graph_distance_cm[2] = distance_cm;
  }

  int pace = 0;
  if (s_graph_distance_count == 3) {
    int64_t delta_cm =
        s_graph_distance_cm[2] - s_graph_distance_cm[0];

    if (delta_cm > 0) {
      int64_t seconds_km =
          (int64_t) (2 * GRAPH_SAMPLE_SECONDS) * 100000 / delta_cm;

      if (seconds_km > 0 && seconds_km <= PACE_MAX_SECONDS) {
        pace = (int) seconds_km;
      }
    }
  }

  if (pace > 0) {
    if (s_graph_best_pace == 0 || pace < s_graph_best_pace) {
      s_graph_best_pace = pace;
    }
    if (pace > s_graph_slowest_pace) {
      s_graph_slowest_pace = pace;
    }
  }

  if (s_graph_count == GRAPH_POINT_CAPACITY) {
    workout_graph_compact();
  }

  int bpm = s_heart_rate_available ? s_current_bpm : 0;
  if (bpm < 30 || bpm > 230) {
    bpm = 0;
  }

  s_graph_points[s_graph_count++] = (WorkoutGraphPoint) {
    .active_seconds = (uint32_t) s_elapsed_seconds,
    .bpm = (uint16_t) bpm,
    .pace_seconds_km = (uint16_t) pace
  };
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

// 5 zones d'effort, en pourcentage de la FC maximale (course et
// entrainement ; la marche est decalee d'un cran, voir zones.c) :
//   Zone 1 : moins de 60 %  (tres leger)              -> bleu
//   Zone 2 : 60 a 70 %      (endurance de base)       -> vert
//   Zone 3 : 70 a 80 %      (aerobie / tempo)         -> jaune
//   Zone 4 : 80 a 90 %      (intense / seuil)         -> orange
//   Zone 5 : 90 % et plus   (maximal)                 -> violet vif
// Hysteresis : la couleur ne change qu'apres avoir depasse le seuil de
// quelques bpm, pour eviter les clignotements autour d'une limite de zone.
#define HEART_RATE_ZONE_HYSTERESIS_BPM 3

static int s_heart_rate_zone = -1;

// Seuils superieurs des zones 1 a 4, en pourcentage de la FC maximale.
// Marche : vert des 53 %, puis paliers reguliers (11 points) jusqu'a 86 %.
// Les seuils (pourcentage du maximum, ou reserve cardiaque si la FC au
// repos est reglee) sont calcules dans zones.c.
static int get_zone_upper_limit_bpm(int zone, int max_bpm) {
  return zones_upper_limit_bpm((int) s_selected_activity, zone, max_bpm,
                               s_profile.resting_hr);
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
    COLOR_LEVEL_1,
    COLOR_LEVEL_2,
    COLOR_LEVEL_3,
    COLOR_LEVEL_4,
    COLOR_LEVEL_5
  };

  if (!s_heart_rate_available) {
    s_heart_rate_zone = -1;
    return GColorLightGray;
  }

  return zone_colors[get_heart_rate_zone(bpm)];
}

static void workout_graph_draw(
    GContext *ctx, GRect bounds, int16_t content_width, int16_t top) {
  bool is_hr = s_current_metric == METRIC_HEART_RATE_GRAPH;
  int minimum = 80;
  int maximum = 180;

  if (is_hr) {
    // Garde 80 et 180 comme reperes, mais ne coupe pas une FC hors plage.
    for (int i = 0; i < s_graph_count; i++) {
      int bpm = s_graph_points[i].bpm;
      if (bpm == 0) {
        continue;
      }
      if (bpm < minimum) {
        minimum = bpm;
      }
      if (bpm > maximum) {
        maximum = bpm;
      }
    }
  } else if (s_graph_best_pace > 0) {
    minimum = s_graph_best_pace * 95 / 100;
    maximum = (s_graph_slowest_pace * 105 + 99) / 100;
    if (maximum < minimum + 60) {
      maximum = minimum + 60;
    }
  } else {
    minimum = 0;
    maximum = 60;
  }

  char label[64];
  if (is_hr) {
    snprintf(label, sizeof(label), "%s (bpm)",
             tr(STR_LABEL_HEART_RATE));
  } else {
    snprintf(label, sizeof(label), "%s (min%s)",
             tr(STR_LABEL_PACE), get_pace_unit());
  }

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(
      ctx, label, font,
      GRect(0, top + 2, content_width, 20),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  const int16_t left = is_hr ? 24 : 6;
  const int16_t right = content_width - 6;
  const int16_t graph_top = top + 28;
  const int16_t bottom = bounds.size.h - 8;
  const int16_t height = bottom - graph_top;
  const int range = maximum - minimum;

  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(left, graph_top), GPoint(right, graph_top));
  graphics_draw_line(ctx, GPoint(left, bottom), GPoint(right, bottom));
  graphics_draw_line(
      ctx, GPoint(left, graph_top + height / 2),
      GPoint(right, graph_top + height / 2));

  if (is_hr) {
    int16_t y180 = bottom - (180 - minimum) * height / range;
    int16_t y80 = bottom - (80 - minimum) * height / range;
    graphics_draw_text(
        ctx, "180", font, GRect(0, y180 - 8, 22, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(
        ctx, "80", font, GRect(0, y80 - 12, 22, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  bool previous_valid = false;
  bool any_valid = false;
  bool latest_valid = false;
  GPoint previous = GPoint(0, 0);
  GPoint latest = GPoint(0, 0);
  int duration = s_elapsed_seconds > 0 ? s_elapsed_seconds : 1;

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_context_set_fill_color(ctx, GColorBlack);

  for (int i = 0; i < s_graph_count; i++) {
    const WorkoutGraphPoint *sample = &s_graph_points[i];
    int value = is_hr ? sample->bpm : sample->pace_seconds_km;

    latest_valid = value > 0;
    if (!latest_valid) {
      previous_valid = false;
      continue;
    }

    int16_t x = left + (int64_t) sample->active_seconds
        * (right - left) / duration;
    int16_t y = is_hr
        ? bottom - (value - minimum) * height / range
        : graph_top + (value - minimum) * height / range;

    GPoint point = GPoint(x, y);
    if (previous_valid) {
      graphics_draw_line(ctx, previous, point);
    } else {
      graphics_fill_circle(ctx, point, 1);
    }

    previous = point;
    latest = point;
    previous_valid = true;
    any_valid = true;
  }

  if (latest_valid) {
    graphics_fill_circle(ctx, latest, 2);
  }

  if (!any_valid) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(
        ctx, "--", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(left, graph_top + height / 2 - 16, right - left, 32),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  graphics_context_set_stroke_width(ctx, 1);
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
      int distance_hundredths = get_distance_hundredths(distance_meters);
      int whole_km = distance_hundredths / 100;
      int decimal_hundredths = distance_hundredths % 100;

      snprintf(
          s_metric_value_text,
          sizeof(s_metric_value_text),
          "%d.%02d",
          whole_km,
          decimal_hundredths);

      snprintf(
          s_metric_unit_text,
          sizeof(s_metric_unit_text),
          "%s",
          get_distance_unit());

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
      if (is_pace_displayable(distance_meters)) {
        // Allure en secondes par kilometre, calculee en entiers uniquement.
        long pace_seconds_per_km =
            get_pace_seconds(distance_meters);

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
            "%s",
            get_pace_unit());
      } else {
        snprintf(
            s_metric_value_text,
            sizeof(s_metric_value_text),
            "--:--");

        snprintf(
            s_metric_unit_text,
            sizeof(s_metric_unit_text),
            "%s",
            get_pace_unit());
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
            get_speed_hundredths(distance_meters);

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
          "%s",
          get_speed_unit());

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
  const int16_t side_bar_width = SIDE_BAR_WIDTH;
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

  graphics_context_set_text_color(ctx, get_text_color_for_background(heart_rate_color));
  GFont clock_font = s_clock_font;
  GSize clock_size = graphics_text_layout_get_content_size(
      s_clock_text,
      clock_font,
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
      clock_font,
      GRect(0, clock_y, content_width, clock_size.h),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter,
      NULL);

  if (s_clock_suffix_text[0] != '\0') {
    GFont suffix_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    GSize suffix_size = graphics_text_layout_get_content_size(
        s_clock_suffix_text, suffix_font, GRect(0, 0, 60, 24),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    // Position fixe, calee sur une heure a 3 chiffres (1 h a 9 h) : le
    // AM/PM ne bouge pas et ne deborde pas de 10 h a 12 h.
    GSize ref_size = graphics_text_layout_get_content_size(
        "8:88", clock_font, GRect(0, 0, 400, half_height),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
    int16_t suffix_x = (content_width + ref_size.w) / 2 -
                       suffix_size.w + CLOCK_AMPM_OFFSET_X;
    int16_t suffix_y = clock_y + clock_size.h -
                       suffix_size.h + CLOCK_AMPM_OFFSET_Y;

    graphics_draw_text(
        ctx, s_clock_suffix_text, suffix_font,
        GRect(suffix_x, suffix_y, suffix_size.w + 4, suffix_size.h),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  if (s_current_metric == METRIC_HEART_RATE_GRAPH ||
      s_current_metric == METRIC_PACE_GRAPH) {
    workout_graph_draw(ctx, bounds, content_width, half_height);
  } else {
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

  }

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

// Debut effectif de la seance : l'heure est prise une seule fois ici, puis
// reutilisee pour la frise, les succes et le tableau des 7 jours.
static void journal_zone_pass_reset(void) {
  s_journal_zone_event = -1;
  s_journal_zone_event_second = 0;
  s_journal_zone_pass_seconds = 0;
  s_journal_zone_pass_index = -1;
}

static void journal_session_begin(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  memset(&s_journal, 0, sizeof(s_journal));
  s_session_pending_save = false;
  memset(s_journal_zone_duration, 0, sizeof(s_journal_zone_duration));
  journal_zone_pass_reset();
  s_journal_redline_current = 0;
  s_journal_zone_event = -1;
  s_journal_zone_event_second = 0;
  s_journal_zone_events = 0;
  s_journal_borne_done = 0;
  s_journal_borne_events = 0;
  s_journal_flame_done = 0;
  s_journal_flame_events = 0;
  s_journal.activity = (uint8_t) s_selected_activity;
  s_journal.start_time = (uint32_t) now;
  s_session_start_time = now;
  s_session_end_time = 0;
  s_session_total_at_stop = 0;
  s_session_units_imperial = s_stride.units_imperial != 0;
  int max_bpm = get_max_heart_rate();
  for (int zone = 0; zone < 4; zone++) {
    s_session_zone_limits[zone] = zones_upper_limit_bpm(
        (int) s_journal.activity, zone, max_bpm, s_profile.resting_hr);
  }
  s_session_start_hour = t->tm_hour;
  s_session_start_day = journal_day_number(
      t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
  s_zone_run_seconds = 0;
}

static void alerts_reset(void) {
  s_alert_km_done = 0;
  s_alert_interval_done = 0;
  s_alert_zone = -1;
  s_alert_zone_second = -ALERT_ZONE_MIN_GAP_SECONDS;
}

#define ALERT_TONE_VOLUME 90

// Kilometre : deux notes montantes (sol puis do aigu). Zone : un bip court a 1000 Hz.
static const SpeakerNote s_alert_km_notes[] = {
  { .midi_note = 79, .waveform = SpeakerWaveformSine, .duration_ms = 150 },
  { .midi_note = 84, .waveform = SpeakerWaveformSine, .duration_ms = 250 }
};

static void alert_play_sound(bool is_km) {
  if (is_km) {
    speaker_play_notes(s_alert_km_notes, 2, ALERT_TONE_VOLUME);
  } else {
    speaker_play_tone(1000, 250, ALERT_TONE_VOLUME, SpeakerWaveformSine);
  }
}

// Motifs : alternance vibration / pause, en millisecondes.
// Premier indice : zone, distance, intervalle.
// Deuxieme indice : discret, normal, renforce.
static void alert_vibrate(int kind) {
  static uint32_t patterns[3][3][5] = {
    {
      {180, 0, 0, 0, 0},
      {320, 0, 0, 0, 0},
      {450, 150, 450, 0, 0}
    },
    {
      {150, 120, 150, 0, 0},
      {300, 150, 300, 0, 0},
      {450, 150, 450, 150, 450}
    },
    {
      {400, 0, 0, 0, 0},
      {650, 0, 0, 0, 0},
      {700, 200, 350, 0, 0}
    }
  };
  static const uint8_t lengths[3][3] = {
    {1, 1, 3},
    {3, 3, 5},
    {1, 1, 3}
  };

  if (kind < 0 || kind > 2) {
    return;
  }
  int level = s_alerts.vibration_level;
  if (level < 0 || level > 2) {
    level = 1;
  }

  VibePattern pattern = {
    .durations = patterns[kind][level],
    .num_segments = lengths[kind][level]
  };
  vibes_enqueue_custom_pattern(pattern);
}

static void alert_fire(bool vibrate, bool sound, bool is_km) {
  if (vibrate) {
    alert_vibrate(is_km ? 1 : 0);
  }

  if (sound) {
    alert_play_sound(is_km);
  }
}

static void alert_fire_interval(bool vibrate, bool sound) {
  if (vibrate) {
    alert_vibrate(2);
  }

  if (sound) {
    speaker_play_tone(660, 400, ALERT_TONE_VOLUME, SpeakerWaveformSine);
  }
}

static void check_alerts(void) {
  if (s_alerts.interval_minutes > 0 &&
      (s_alerts.vibrate_interval || s_alerts.sound_interval)) {
    int interval_count =
        s_elapsed_seconds / (s_alerts.interval_minutes * 60);

    if (interval_count > s_alert_interval_done) {
      s_alert_interval_done = interval_count;
      alert_fire_interval(s_alerts.vibrate_interval,
                          s_alerts.sound_interval);
    }
  }

  if (s_selected_activity != ACTIVITY_TRAINING &&
      (s_alerts.vibrate_km || s_alerts.sound_km)) {
    int unit_count = get_distance_hundredths(get_distance_meters()) / 100;

    if (unit_count > s_alert_km_done) {
      s_alert_km_done = unit_count;
      alert_fire(s_alerts.vibrate_km, s_alerts.sound_km, true);
    }
  }

  if (!(s_alerts.vibrate_zone || s_alerts.sound_zone) ||
      !s_heart_rate_available) {
    return;
  }

  int zone = get_heart_rate_zone(s_current_bpm);

  if (s_alert_zone < 0) {
    s_alert_zone = zone;
    return;
  }

  if (zone != s_alert_zone &&
      s_elapsed_seconds - s_alert_zone_second >=
          ALERT_ZONE_MIN_GAP_SECONDS) {
    s_alert_zone = zone;
    s_alert_zone_second = s_elapsed_seconds;
    alert_fire(s_alerts.vibrate_zone, s_alerts.sound_zone, false);
  }
}

static void journal_track_tick(void) {
  // Zones : totaux, maximum redline et passages d'au moins 120 s.
  // Un evenement par passage ; sa duree reste en memoire.
  if (s_heart_rate_available && s_heart_rate_zone >= 0 &&
      s_heart_rate_zone < JOURNAL_ZONE_COUNT) {
    s_journal.zone_seconds[s_heart_rate_zone]++;

    if (s_heart_rate_zone >= 3) {
      if (s_journal_redline_current < UINT16_MAX) {
        s_journal_redline_current++;
      }
      if (s_journal_redline_current > s_journal.redline_seconds) {
        s_journal.redline_seconds = s_journal_redline_current;
      }
    } else {
      s_journal_redline_current = 0;
    }

    if (s_heart_rate_zone != s_journal_zone_event) {
      journal_zone_pass_reset();
      s_journal_zone_event = s_heart_rate_zone;
      // Le tick courant represente la seconde qui vient de s'ecouler.
      s_journal_zone_event_second =
          s_elapsed_seconds > 0 ? s_elapsed_seconds - 1 : 0;
    }

    if (s_journal_zone_pass_seconds < UINT16_MAX) {
      s_journal_zone_pass_seconds++;
    }

    if (s_journal_zone_pass_index < 0 &&
        s_journal_zone_pass_seconds >= 120 &&
        s_journal.event_count < JOURNAL_MAX_EVENTS &&
        s_journal_zone_events < JOURNAL_MAX_ZONE_EVENTS) {
      int index = s_journal.event_count;
      if (journal_add_event(
              &s_journal, JE_ZONE,
              (uint16_t) s_journal_zone_event_second,
              (uint8_t) s_heart_rate_zone)) {
        s_journal_zone_pass_index = index;
        s_journal_zone_events++;
      }
    }

    if (s_journal_zone_pass_index >= 0) {
      s_journal_zone_duration[s_journal_zone_pass_index] =
          s_journal_zone_pass_seconds;
    }
  } else {
    journal_zone_pass_reset();
    s_journal_redline_current = 0;
  }

  // Bornes : marche 1 km ou 1 mile, course 2 km ou 2 miles.
  if (s_selected_activity != ACTIVITY_TRAINING) {
    int borne_m = (s_session_units_imperial ? 1609 : 1000) *
        ((s_selected_activity == ACTIVITY_RUN) ? 2 : 1);
    int borne_count = get_distance_meters() / borne_m;

    if (borne_count > s_journal_borne_done &&
        s_journal.event_count < JOURNAL_MAX_EVENTS &&
        s_journal_borne_events < JOURNAL_MAX_BORNE_EVENTS) {
      s_journal_borne_done = borne_count;
      journal_add_event(&s_journal, JE_BORNE,
                        (uint16_t) s_elapsed_seconds,
                        (uint8_t) s_journal_borne_done);
      s_journal_borne_events++;
    }
  }

  // Flammes : une tous les 200 kcal, seulement avec la frequence cardiaque.
  if (s_heart_rate_available) {
    int flame_count = get_session_calories() / 200;

    if (flame_count > s_journal_flame_done &&
        s_journal.event_count < JOURNAL_MAX_EVENTS &&
        s_journal_flame_events < JOURNAL_MAX_FLAME_EVENTS) {
      s_journal_flame_done = flame_count;
      journal_add_event(&s_journal, JE_FLAME,
                        (uint16_t) s_elapsed_seconds,
                        (uint8_t) s_journal_flame_done);
      s_journal_flame_events++;
    }
  }
}

static void workout_tick_handler(struct tm *tick_time,
                                 TimeUnits units_changed) {
  if (!s_is_paused) {
    s_elapsed_seconds++;
    refresh_heart_rate_if_needed();
    refresh_steps_if_needed();
    workout_graph_sample();
    check_alerts();
    journal_track_tick();
  } else {
    s_paused_seconds++;
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
  journal_session_begin();
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
  s_menu_selection =
      (s_menu_selection + MENU_ROWS - 1) % MENU_ROWS;

  layer_mark_dirty(s_menu_layer);
}

static void menu_down_click_handler(ClickRecognizerRef recognizer,
                                    void *context) {
  s_menu_selection = (s_menu_selection + 1) % MENU_ROWS;

  layer_mark_dirty(s_menu_layer);
}

static void menu_select_click_handler(ClickRecognizerRef recognizer,
                                      void *context) {
  if (s_menu_selection < ACTIVITY_COUNT) {
    s_selected_activity = (Activity) s_menu_selection;
    start_countdown();
  } else {
    window_stack_push(s_journal_window, true);
  }
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

// Garde la police principale ; dessine uniquement AM/PM en petit.
static void draw_clock_small_suffix(
    GContext *ctx, const char *text, GFont main_font,
    GRect frame, GTextAlignment alignment) {
  char main_text[64];
  snprintf(main_text, sizeof(main_text), "%s", text);

  size_t length = strlen(main_text);
  const char *suffix = NULL;
  if (length >= 3) {
    const char *tail = main_text + length - 3;
    if (strcmp(tail, " AM") == 0) {
      suffix = "AM";
    } else if (strcmp(tail, " PM") == 0) {
      suffix = "PM";
    }
  }

  if (suffix == NULL) {
    graphics_draw_text(
        ctx, text, main_font, frame,
        GTextOverflowModeTrailingEllipsis, alignment, NULL);
    return;
  }

  main_text[length - 3] = '\0';
  GFont suffix_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GSize main_size = graphics_text_layout_get_content_size(
      main_text, main_font, GRect(0, 0, 1000, frame.size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  GSize suffix_size = graphics_text_layout_get_content_size(
      suffix, suffix_font, GRect(0, 0, 100, frame.size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

  const int gap = 3;
  int width = main_size.w + gap + suffix_size.w;
  int x = frame.origin.x;
  if (alignment == GTextAlignmentRight) {
    x += frame.size.w - width;
  } else if (alignment == GTextAlignmentCenter) {
    x += (frame.size.w - width) / 2;
  }
  if (x < frame.origin.x) {
    x = frame.origin.x;
  }

  int suffix_y = frame.origin.y + main_size.h - suffix_size.h;
  if (suffix_y < frame.origin.y) {
    suffix_y = frame.origin.y;
  }

  graphics_draw_text(
      ctx, main_text, main_font,
      GRect(x, frame.origin.y, main_size.w + 2, frame.size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(
      ctx, suffix, suffix_font,
      GRect(x + main_size.w + gap, suffix_y, suffix_size.w + 2, 20),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

// Separe uniquement l'unite de duree " min", pas "min/km".
static bool split_duration_min(
    const char *text, char *before, char *after) {
  const char *unit = strstr(text, " min");
  if (unit == NULL || (unit[4] != '\0' && unit[4] != ' ') ||
      strlen(text) >= 128) {
    return false;
  }

  size_t length = (size_t) (unit - text);
  memcpy(before, text, length);
  before[length] = '\0';
  snprintf(after, 128, "%s", unit + 4);
  return true;
}

static GSize text_small_min_size(
    const char *text, GFont font, GRect frame,
    GTextOverflowMode overflow, GTextAlignment alignment) {
  char before[128];
  char after[128];
  if (overflow != GTextOverflowModeTrailingEllipsis ||
      !split_duration_min(text, before, after)) {
    return graphics_text_layout_get_content_size(
        text, font, frame, overflow, alignment);
  }

  GRect measure = GRect(0, 0, 1000, frame.size.h);
  GFont small = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GSize a = graphics_text_layout_get_content_size(
      before, font, measure, overflow, GTextAlignmentLeft);
  GSize b = graphics_text_layout_get_content_size(
      "min", small, measure, overflow, GTextAlignmentLeft);
  GSize c = graphics_text_layout_get_content_size(
      after, font, measure, overflow, GTextAlignmentLeft);

  int height = a.h > c.h ? a.h : c.h;
  if (b.h > height) {
    height = b.h;
  }
  return GSize(a.w + 3 + b.w + c.w, height);
}

static void draw_text_small_min(
    GContext *ctx, const char *text, GFont font, GRect frame,
    GTextOverflowMode overflow, GTextAlignment alignment) {
  char before[128];
  char after[128];
  if (overflow != GTextOverflowModeTrailingEllipsis ||
      !split_duration_min(text, before, after)) {
    graphics_draw_text(ctx, text, font, frame, overflow, alignment, NULL);
    return;
  }

  GRect measure = GRect(0, 0, 1000, frame.size.h);
  GFont small = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GSize a = graphics_text_layout_get_content_size(
      before, font, measure, overflow, GTextAlignmentLeft);
  GSize b = graphics_text_layout_get_content_size(
      "min", small, measure, overflow, GTextAlignmentLeft);
  GSize c = graphics_text_layout_get_content_size(
      after, font, measure, overflow, GTextAlignmentLeft);

  int width = a.w + 3 + b.w + c.w;
  int x = frame.origin.x;
  if (alignment == GTextAlignmentRight) {
    x += frame.size.w - width;
  } else if (alignment == GTextAlignmentCenter) {
    x += (frame.size.w - width) / 2;
  }
  if (x < frame.origin.x) {
    x = frame.origin.x;
  }

  int main_height = a.h > c.h ? a.h : c.h;
  int small_y = frame.origin.y + main_height - b.h;
  if (small_y < frame.origin.y) {
    small_y = frame.origin.y;
  }
  int right = frame.origin.x + frame.size.w;

  if (before[0] != '\0' && x < right) {
    graphics_draw_text(
        ctx, before, font,
        GRect(x, frame.origin.y, right - x, frame.size.h),
        overflow, GTextAlignmentLeft, NULL);
  }
  x += a.w + 3;
  if (x < right) {
    graphics_draw_text(
        ctx, "min", small,
        GRect(x, small_y, right - x, 24),
        overflow, GTextAlignmentLeft, NULL);
  }
  x += b.w;
  if (after[0] != '\0' && x < right) {
    graphics_draw_text(
        ctx, after, font,
        GRect(x, frame.origin.y, right - x, frame.size.h),
        overflow, GTextAlignmentLeft, NULL);
  }
}

static void journal_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int16_t first_row_y = 36;
  const int16_t compact_height = 32;
  const int16_t expanded_height = 64;
  GFont date_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont small_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont detail_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);

  for (int offset = 0; offset <= 1; offset++) {
    draw_text_small_min(
        ctx, "Journal",
        fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
        GRect(offset, 4, bounds.size.w, 32),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
  }

  if (s_journal_count == 0) {
    draw_text_small_min(
        ctx, tr(STR_NO_SESSIONS), date_font,
        GRect(6, 100, bounds.size.w - 12, 40),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
    return;
  }

  // Reserve la place d'une ligne selectionnee depliee.
  int available_height = bounds.size.h - first_row_y;
  int visible_rows =
      (available_height - expanded_height) / compact_height + 1;
  if (visible_rows < 1) {
    visible_rows = 1;
  }

  if (s_journal_selection < s_journal_first_visible) {
    s_journal_first_visible = s_journal_selection;
  } else if (s_journal_selection >=
             s_journal_first_visible + visible_rows) {
    s_journal_first_visible = s_journal_selection - visible_rows + 1;
  }

  int max_first = s_journal_count - visible_rows;
  if (max_first < 0) {
    max_first = 0;
  }
  if (s_journal_first_visible > max_first) {
    s_journal_first_visible = max_first;
  }
  if (s_journal_first_visible < 0) {
    s_journal_first_visible = 0;
  }

  int last_visible = s_journal_first_visible + visible_rows;
  if (last_visible > s_journal_count) {
    last_visible = s_journal_count;
  }

  int16_t row_y = first_row_y;
  for (int i = s_journal_first_visible; i < last_visible; i++) {
    bool selected = i == s_journal_selection;
    int16_t row_height = selected ? expanded_height : compact_height;
    char date_text[12];
    char time_text[12];
    char duration_text[16];

    time_t start_time = (time_t) s_journal_list[i].start_time;
    struct tm *start_tm = localtime(&start_time);

    if (start_tm == NULL ||
        strftime(date_text, sizeof(date_text), "%d/%m", start_tm) == 0) {
      snprintf(date_text, sizeof(date_text), "--/--");
    }
    timeline_format_clock(start_time, time_text, sizeof(time_text));

    if (selected) {
      graphics_context_set_fill_color(ctx, ACCENT_YELLOW);
      graphics_fill_rect(
          ctx, GRect(4, row_y, bounds.size.w - 8, row_height - 1),
          4, GCornersAll);
    }

    graphics_context_set_text_color(ctx, GColorBlack);
    draw_text_small_min(
        ctx, date_text, date_font,
        GRect(12, row_y, 84, 32),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    draw_clock_small_suffix(
        ctx, time_text, date_font,
        GRect(bounds.size.w - 96, row_y, 84, 32),
        GTextAlignmentRight);

    if (selected) {
      snprintf(
          duration_text, sizeof(duration_text), "%u min",
          (unsigned) (s_journal_list[i].active_seconds / 60));

      GSize duration_size = text_small_min_size(
          duration_text, detail_font, GRect(0, 0, 200, 32),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentRight);

      int16_t duration_width = duration_size.w + 4;
      int16_t duration_x = bounds.size.w - 12 - duration_width;
      int16_t activity_width = duration_x - 12 - 8;

      const char *activity =
          tr(STR_ACTIVITY_WALK + s_journal_list[i].activity);
      GFont activity_font = detail_font;

      GSize activity_size = text_small_min_size(
          activity, activity_font, GRect(0, 0, 400, 32),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

      if (activity_size.w + 1 > activity_width) {
        activity_font = small_font;
      }

      for (int offset = 0; offset <= 1; offset++) {
        draw_text_small_min(
            ctx, activity, activity_font,
            GRect(12 + offset, row_y + 32, activity_width, 32),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      }

      draw_text_small_min(
          ctx, duration_text, detail_font,
          GRect(duration_x, row_y + 32, duration_width, 32),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentRight);
    }

    row_y += row_height;
  }
}

static void journal_up_click_handler(ClickRecognizerRef recognizer,
                                     void *context) {
  if (s_journal_count == 0) {
    return;
  }

  s_journal_selection =
      (s_journal_selection + s_journal_count - 1) % s_journal_count;
  layer_mark_dirty(s_journal_layer);
}

static void journal_down_click_handler(ClickRecognizerRef recognizer,
                                       void *context) {
  if (s_journal_count == 0) {
    return;
  }

  s_journal_selection = (s_journal_selection + 1) % s_journal_count;
  layer_mark_dirty(s_journal_layer);
}

static void journal_back_click_handler(ClickRecognizerRef recognizer,
                                       void *context) {
  window_stack_pop(true);
}

static void journal_select_click_handler(
    ClickRecognizerRef recognizer, void *context) {
  if (s_journal_count == 0) {
    return;
  }

  s_detail_session = s_journal_list[s_journal_selection];
  s_detail_scroll_offset = 0;
  window_stack_push(s_detail_window, true);
}

static void journal_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, journal_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, journal_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, journal_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, journal_back_click_handler);
}

static void journal_window_load(Window *window) {
  JournalIndex index;
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, GColorWhite);

  journal_load_index(&index);
  s_journal_count = index.count;

  if (s_journal_selection >= s_journal_count) {
    s_journal_selection = 0;
  }

  int readable_count = 0;

  for (int i = 0; i < s_journal_count; i++) {
    int slot = journal_index_slot_by_age(&index, i);

    if (slot >= 0 &&
        journal_load_session(slot, &s_journal_list[readable_count])) {
      readable_count++;
    }
  }

  s_journal_count = readable_count;

  s_journal_layer = layer_create(bounds);
  layer_set_update_proc(s_journal_layer, journal_layer_update_proc);
  layer_add_child(window_layer, s_journal_layer);
}

static void journal_window_unload(Window *window) {
  layer_destroy(s_journal_layer);
  s_journal_layer = NULL;
}

static void draw_summary_line(GContext *ctx, int16_t y,
                              const char *value, const char *unit,
                              const char *label, GFont value_font);

static void detail_scroll_timer_handler(void *context) {
  s_detail_scroll_timer = NULL;
  s_detail_scroll_frame++;

  if (s_detail_scroll_frame >= DETAIL_SCROLL_FRAMES) {
    s_detail_scroll_offset = s_detail_scroll_to;
    s_detail_scroll_frame = 0;
    layer_mark_dirty(s_detail_layer);
    return;
  }

  int range = s_detail_scroll_to - s_detail_scroll_from;
  s_detail_scroll_offset =
      s_detail_scroll_from + range * s_detail_scroll_frame
      / DETAIL_SCROLL_FRAMES;

  layer_mark_dirty(s_detail_layer);
  s_detail_scroll_timer = app_timer_register(
      DETAIL_SCROLL_FRAME_MS,
      detail_scroll_timer_handler,
      NULL);
}

static void detail_start_scroll_animation(int target_offset) {
  detail_cancel_scroll_animation();

  s_detail_scroll_from = s_detail_scroll_offset;
  s_detail_scroll_to = target_offset;

  if (s_detail_scroll_from == s_detail_scroll_to) {
    return;
  }

  s_detail_scroll_frame = 0;
  s_detail_scroll_timer = app_timer_register(
      DETAIL_SCROLL_FRAME_MS,
      detail_scroll_timer_handler,
      NULL);
}

static void detail_cancel_scroll_animation(void) {
  if (s_detail_scroll_timer != NULL) {
    app_timer_cancel(s_detail_scroll_timer);
    s_detail_scroll_timer = NULL;
  }

  s_detail_scroll_frame = 0;
  s_detail_scroll_from = s_detail_scroll_offset;
  s_detail_scroll_to = s_detail_scroll_offset;
}

static void detail_up_click_handler(
    ClickRecognizerRef recognizer, void *context) {
  int target = s_detail_scroll_offset - SUMMARY_SCROLL_STEP;

  if (target < 0) {
    target = 0;
  }

  detail_start_scroll_animation(target);
}

static void detail_down_click_handler(
    ClickRecognizerRef recognizer, void *context) {
  GRect bounds = layer_get_bounds(s_detail_layer);
  int max_offset = s_detail_content_height - bounds.size.h;

  if (max_offset < 0) {
    max_offset = 0;
  }

  int target = s_detail_scroll_offset + SUMMARY_SCROLL_STEP;

  if (target > max_offset) {
    target = max_offset;
  }

  detail_start_scroll_animation(target);
}

static void draw_detail_line(GContext *ctx, int16_t y,
                             const char *value, const char *unit,
                             const char *label, GFont value_font) {
  GSize value_size = text_small_min_size(
      value,
      value_font,
      GRect(0, 0, 400, 50),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);

  graphics_context_set_text_color(ctx, GColorBlack);
  draw_text_small_min(
      ctx,
      label,
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(6, y, 188, 22),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);

  graphics_context_set_text_color(ctx, GColorBlack);
  if (strcmp(label, tr(STR_LABEL_DATE)) == 0) {
    draw_clock_small_suffix(
        ctx, value, value_font, GRect(6, y + 22, 188, 44),
        GTextAlignmentLeft);
  } else {
    draw_text_small_min(
        ctx,
        value,
        value_font,
        GRect(6, y + 22, 188, 44),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
  }

  if (unit[0] != '\0') {
    draw_text_small_min(
        ctx,
        unit,
        fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
        GRect(6 + value_size.w + 6, y + 22 + SUMMARY_UNIT_Y_OFFSET, 110, 34),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
  }

  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(6, y + 70), GPoint(194, y + 70));
}

static void draw_detail_intensity(GContext *ctx, int16_t y, int level) {
  const GColor segment_colors[5] = {
    COLOR_LEVEL_1,
    COLOR_LEVEL_2,
    COLOR_LEVEL_3,
    COLOR_LEVEL_4,
    COLOR_LEVEL_5
  };
  const int16_t segment_width = 30;
  const int16_t segment_height = 20;
  const int16_t segment_gap = 4;
  const int16_t segment_y = y + 26;

  for (int i = 0; i < 5; i++) {
    int16_t x = 6 + i * (segment_width + segment_gap);

    graphics_context_set_fill_color(
        ctx, (i < level) ? segment_colors[i] : GColorLightGray);
    graphics_fill_rect(
        ctx,
        GRect(x, segment_y, segment_width, segment_height),
        2,
        GCornersAll);
  }

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(
      ctx,
      tr(STR_SUMMARY_INTENSITY),
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(6, y, 188, 22),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft,
      NULL);

  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(6, y + 70), GPoint(194, y + 70));
}

typedef enum {
  TIMELINE_MARKER_FEELING = 0,
  TIMELINE_MARKER_ACHIEVEMENT,
  TIMELINE_MARKER_START,
  TIMELINE_MARKER_ZONE,
  TIMELINE_MARKER_DISTANCE,
  TIMELINE_MARKER_CALORIES,
  TIMELINE_MARKER_END
} TimelineMarker;

static void timeline_draw_small_star(
    GContext *ctx, GPoint center, GColor color) {
  GPoint points[] = {
    {6, 0}, {8, 4}, {12, 4}, {9, 7}, {10, 12},
    {6, 9}, {2, 12}, {3, 7}, {0, 4}, {4, 4}
  };
  GPathInfo info = {
    .num_points = sizeof(points) / sizeof(points[0]),
    .points = points
  };
  GPath *path = gpath_create(&info);
  if (path != NULL) {
    graphics_context_set_fill_color(ctx, color);
    gpath_move_to(path, GPoint(center.x - 6, center.y - 6));
    gpath_draw_filled(ctx, path);
    gpath_destroy(path);
  }
}

static void timeline_draw_marker(GContext *ctx, GPoint center,
                                 TimelineMarker marker, GColor color) {
  int16_t x = center.x - 9;
  int16_t y = center.y - 9;
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);

  if (marker == TIMELINE_MARKER_ACHIEVEMENT) {
    // Deux rubans au-dessus du medaillon.
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_line(
        ctx, GPoint(center.x - 5, center.y - 9),
        GPoint(center.x - 1, center.y - 3));
    graphics_draw_line(
        ctx, GPoint(center.x + 5, center.y - 9),
        GPoint(center.x + 1, center.y - 3));

    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_circle(ctx, GPoint(center.x, center.y + 3), 6);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_circle(ctx, GPoint(center.x, center.y + 3), 3);
    return;
  }

  if (marker == TIMELINE_MARKER_START ||
      marker == TIMELINE_MARKER_END) {
    // Mat commun aux drapeaux de depart et d'arrivee.
    graphics_draw_line(ctx, GPoint(x + 3, y + 1),
                       GPoint(x + 3, y + 17));
    graphics_context_set_stroke_width(ctx, 1);

    if (marker == TIMELINE_MARKER_START) {
      graphics_fill_rect(ctx, GRect(x + 4, y + 1, 12, 8),
                         0, GCornerNone);
    } else {
      // Damier : quatre colonnes et deux rangees.
      for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 4; col++) {
          if ((row + col) % 2 == 0) {
            graphics_fill_rect(
                ctx, GRect(x + 4 + col * 3, y + 1 + row * 4, 3, 4),
                0, GCornerNone);
          }
        }
      }
      graphics_draw_rect(ctx, GRect(x + 4, y + 1, 12, 8));
    }

    return;
  }

  if (marker == TIMELINE_MARKER_FEELING) {
    graphics_draw_circle(ctx, center, 8);
    graphics_fill_circle(ctx, GPoint(x + 6, y + 6), 1);
    graphics_fill_circle(ctx, GPoint(x + 12, y + 6), 1);
    graphics_draw_line(ctx, GPoint(x + 5, y + 11),
                       GPoint(x + 8, y + 14));
    graphics_draw_line(ctx, GPoint(x + 8, y + 14),
                       GPoint(x + 10, y + 14));
    graphics_draw_line(ctx, GPoint(x + 10, y + 14),
                       GPoint(x + 13, y + 11));
    graphics_context_set_stroke_width(ctx, 1);
    return;
  }

  if (marker == TIMELINE_MARKER_DISTANCE) {
    graphics_draw_round_rect(ctx, GRect(x + 3, y + 1, 12, 16), 3);
    graphics_draw_line(ctx, GPoint(x + 6, y + 6),
                       GPoint(x + 12, y + 6));
    graphics_draw_line(ctx, GPoint(x + 6, y + 11),
                       GPoint(x + 12, y + 11));
    graphics_context_set_stroke_width(ctx, 1);
    return;
  }

  GPoint points[12];
  uint32_t count = 0;

  if (marker == TIMELINE_MARKER_ZONE) {
    GPoint shape[] = {
      {9, 5}, {6, 1}, {2, 1}, {0, 5}, {1, 9},
      {9, 17}, {17, 9}, {18, 5}, {16, 1}, {12, 1}
    };
    count = sizeof(shape) / sizeof(shape[0]);
    for (uint32_t i = 0; i < count; i++) {
      points[i] = shape[i];
    }
  } else if (marker == TIMELINE_MARKER_CALORIES) {
    GPoint shape[] = {
      {10, 0}, {10, 6}, {14, 4}, {17, 10}, {16, 14},
      {12, 18}, {6, 18}, {2, 14}, {1, 10}, {5, 5}, {5, 10}
    };
    count = sizeof(shape) / sizeof(shape[0]);
    for (uint32_t i = 0; i < count; i++) {
      points[i] = shape[i];
    }
  }

  if (count > 0) {
    GPathInfo info = {
      .num_points = count,
      .points = points
    };
    GPath *path = gpath_create(&info);
    if (path != NULL) {
      gpath_move_to(path, GPoint(x, y));
      gpath_draw_filled(ctx, path);
      gpath_destroy(path);
    }
  }

  graphics_context_set_stroke_width(ctx, 1);
}

// Liste des evenements affichables, tries par temps actif.
// Chaque passage de zone conserve son evenement distinct.
static int timeline_event_indices(const JournalSession *session,
                                  uint8_t *indices) {
  int count = 0;
  int event_count = session->event_count;

  if (event_count > JOURNAL_MAX_EVENTS) {
    event_count = JOURNAL_MAX_EVENTS;
  }

  for (int i = 0; i < event_count; i++) {
    const JournalEvent *event = &session->events[i];

    if (event->type == JE_ZONE) {
      if (event->value >= JOURNAL_ZONE_COUNT) {
        continue;
      }
    } else if (event->type == JE_BORNE || event->type == JE_FLAME) {
      if (event->value == 0) {
        continue;
      }
    } else {
      continue;
    }

    // Tri stable : le passage de zone est confirme apres 60 s,
    // mais son horodatage designe son debut.
    int position = count;
    while (position > 0 &&
           session->events[indices[position - 1]].seconds > event->seconds) {
      indices[position] = indices[position - 1];
      position--;
    }
    indices[position] = (uint8_t) i;
    count++;
  }

  return count;
}


static int timeline_heading_height(bool achievement) {
  const char *title = "";
  if (achievement) {
    int ach = journal_top_achievement(s_journal.achievements);
    if (ach >= 0) {
      title = tr(STR_ACHIEVEMENT_WELCOME_BACK + ach);
    }
  } else if (s_journal.feeling >= 1 && s_journal.feeling <= 4) {
    title = tr(STR_EFFORT_RELAXED + s_journal.feeling - 1);
  }

  int width = s_timeline_layer != NULL
      ? layer_get_bounds(s_timeline_layer).size.w : 200;
  GFont font = fonts_get_system_font(
      achievement ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_28_BOLD);
  GSize size = graphics_text_layout_get_content_size(
      title, font,
      GRect(0, 0, width - (achievement ? 86 : 42), 1000),
      GTextOverflowModeWordWrap,
      achievement ? GTextAlignmentCenter : GTextAlignmentLeft);

  if (achievement) {
    return size.h + 8 > 36 ? size.h + 8 : 36;
  }
  return size.h > 34 ? size.h : 34;
}

static int timeline_achievement_height(void) {
  int achievement = journal_top_achievement(s_journal.achievements);
  if (achievement < 0) {
    return 62;
  }

  int width = 200;
  if (s_timeline_layer != NULL) {
    width = layer_get_bounds(s_timeline_layer).size.w;
  }

  const char *description = tr(STR_ACH_DESC_WELCOME_BACK + achievement);
  GSize size = graphics_text_layout_get_content_size(
      description,
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(0, 0, width - 42, 200),
      GTextOverflowModeWordWrap,
      GTextAlignmentLeft);

  int height = timeline_heading_height(true) + 20 + size.h + 8;
  return height > 62 ? height : 62;
}

static int timeline_block_height(int index,
                                 bool has_achievement,
                                 int first_event_block,
                                 int event_count,
                                 const uint8_t *indices) {
  if (index == 0) {
    return timeline_heading_height(false) + 20 + 8;
  }
  if (has_achievement && index == 1) {
    return timeline_achievement_height();
  }
  if (index == first_event_block - 1) {
    return 84;
  }
  if (index == first_event_block + event_count) {
    return 84;
  }
  if (index >= first_event_block &&
      index < first_event_block + event_count) {
    const JournalEvent *event =
        &s_journal.events[indices[index - first_event_block]];
    return event->type == JE_ZONE ? TIMELINE_ZONE_HEIGHT : 62;
  }
  return 62;
}

static int timeline_block_offset(int index) {
  uint8_t indices[JOURNAL_MAX_EVENTS];
  int event_count = timeline_event_indices(&s_journal, indices);
  bool has_achievement =
      journal_top_achievement(s_journal.achievements) >= 0;
  int first_event_block = 2 + (has_achievement ? 1 : 0);
  int screen_height = s_timeline_layer != NULL
      ? layer_get_bounds(s_timeline_layer).size.h : 228;
  int page_span = screen_height - TIMELINE_PAGE_TOP
      + TIMELINE_PAGE_SPACING;
  int offset = 0;
  int page_origin = 0;
  int next_page = 1;

  for (int i = 0; i <= index; i++) {
    if (next_page < s_timeline_page_count &&
        i == s_timeline_page_starts[next_page]) {
      int minimum_offset = page_origin + page_span;
      if (offset < minimum_offset) {
        offset = minimum_offset;
      }
      page_origin = offset;
      next_page++;
    }

    if (i == index) {
      return offset;
    }

    offset += timeline_block_height(
        i, has_achievement, first_event_block, event_count, indices)
        + TIMELINE_EVENT_GAP;
  }

  return offset;
}

static void timeline_build_pages(void) {
  uint8_t indices[JOURNAL_MAX_EVENTS];
  int event_count = timeline_event_indices(&s_journal, indices);
  bool has_achievement =
      journal_top_achievement(s_journal.achievements) >= 0;
  int first_event_block = 2 + (has_achievement ? 1 : 0);
  int block_count = first_event_block + event_count + 1;
  int screen_height = layer_get_bounds(s_timeline_layer).size.h;
  int capacity = screen_height - TIMELINE_PAGE_TOP
      - TIMELINE_PAGE_BOTTOM - TIMELINE_KICK_PX;
  int used = 0;

  s_timeline_page_count = 1;
  s_timeline_page_starts[0] = 0;

  for (int i = 0; i < block_count; i++) {
    int height = timeline_block_height(
        i, has_achievement, first_event_block, event_count, indices);
    int gap = used > 0 ? TIMELINE_EVENT_GAP : 0;

    if (used > 0 && used + gap + height > capacity) {
      s_timeline_page_starts[s_timeline_page_count++] = (uint8_t) i;
      used = 0;
      gap = 0;
    }

    used += gap + height;
  }
}

static void timeline_scroll_finish(void) {
  if (s_timeline_scroll_timer != NULL) {
    app_timer_cancel(s_timeline_scroll_timer);
    s_timeline_scroll_timer = NULL;
  }

  s_timeline_stretch = 0;

  if (s_timeline_page_count > 0) {
    s_timeline_scroll_offset = timeline_block_offset(
        s_timeline_page_starts[s_timeline_selection]);
  } else {
    s_timeline_scroll_offset = 0;
  }

  if (s_timeline_layer != NULL) {
    layer_mark_dirty(s_timeline_layer);
  }
}

static void timeline_scroll_tick(void *context) {
  // Depassement, contre-secousse, puis stabilisation.
  static const uint16_t progress[TIMELINE_SCROLL_FRAMES] = {
    20, 70, 160, 300, 480, 680, 850,
    970, 1040, 1060, 1030, 990, 1005, 1000
  };
  static const uint8_t stretch[TIMELINE_SCROLL_FRAMES] = {
    0, 2, 4, 5, 6, 6, 4,
    2, 1, 0, 0, 0, 0, 0
  };

  s_timeline_scroll_timer = NULL;

  if (s_timeline_layer == NULL) {
    s_timeline_stretch = 0;
    return;
  }

  int frame = s_timeline_scroll_frame;
  if (frame >= TIMELINE_SCROLL_FRAMES) {
    timeline_scroll_finish();
    return;
  }

  int32_t distance =
      (int32_t) s_timeline_scroll_target - s_timeline_scroll_from;
  s_timeline_scroll_offset = s_timeline_scroll_from
      + (distance * progress[frame]) / 1000;
  s_timeline_stretch = stretch[frame];
  s_timeline_scroll_frame++;

  if (s_timeline_scroll_frame < TIMELINE_SCROLL_FRAMES) {
    s_timeline_scroll_timer = app_timer_register(
        TIMELINE_SCROLL_FRAME_MS, timeline_scroll_tick, NULL);
    if (s_timeline_scroll_timer == NULL) {
      timeline_scroll_finish();
      return;
    }
  } else {
    s_timeline_scroll_offset = s_timeline_scroll_target;
    s_timeline_stretch = 0;
  }

  layer_mark_dirty(s_timeline_layer);
}

static void timeline_scroll_to_selection(void) {
  if (s_timeline_scroll_timer != NULL) {
    app_timer_cancel(s_timeline_scroll_timer);
    s_timeline_scroll_timer = NULL;
  }

  // En cas d'appui rapide, repart de la position actuellement affichee.
  s_timeline_scroll_from = s_timeline_scroll_offset;
  s_timeline_scroll_target = timeline_block_offset(
      s_timeline_page_starts[s_timeline_selection]);
  s_timeline_scroll_frame = 0;
  s_timeline_stretch = 0;

  if (s_timeline_scroll_from == s_timeline_scroll_target) {
    layer_mark_dirty(s_timeline_layer);
    return;
  }

  s_timeline_scroll_timer = app_timer_register(
      TIMELINE_SCROLL_FRAME_MS, timeline_scroll_tick, NULL);

  if (s_timeline_scroll_timer == NULL) {
    timeline_scroll_finish();
  }
}

static void timeline_format_clock(time_t timestamp,
                                  char *buffer, size_t size) {
  snprintf(buffer, size, "--:--");
  if (timestamp <= 0) {
    return;
  }

  struct tm *clock_tm = localtime(&timestamp);
  if (clock_tm == NULL) {
    return;
  }

  int hour = clock_tm->tm_hour;
  int minute = clock_tm->tm_min;

  if (s_clock.use_12h) {
    const char *period = hour < 12 ? "AM" : "PM";
    hour %= 12;
    if (hour == 0) {
      hour = 12;
    }

    if (s_clock.show_ampm) {
      snprintf(buffer, size, "%d:%02d %s", hour, minute, period);
    } else {
      snprintf(buffer, size, "%d:%02d", hour, minute);
    }
  } else if (s_clock.hide_hour_zero) {
    snprintf(buffer, size, "%d:%02d", hour, minute);
  } else {
    snprintf(buffer, size, "%02d:%02d", hour, minute);
  }
}

static void timeline_format_zone_range(int zone,
                                       char *buffer, size_t size) {
  if (zone < 0 || zone >= JOURNAL_ZONE_COUNT) {
    snprintf(buffer, size, "--");
  } else if (zone == 0) {
    snprintf(buffer, size, "< %d bpm", s_session_zone_limits[0]);
  } else if (zone == 4) {
    snprintf(buffer, size, ">= %d bpm", s_session_zone_limits[3]);
  } else {
    snprintf(buffer, size, "%d - %d bpm",
             s_session_zone_limits[zone - 1],
             s_session_zone_limits[zone] - 1);
  }
}

static void timeline_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int16_t bar_width = 30;
  const int16_t content_width = bounds.size.w - bar_width;
  const int16_t first_block_y = TIMELINE_PAGE_TOP;

  static const GColor level_colors[5] = {
    COLOR_LEVEL_1,
    COLOR_LEVEL_2,
    COLOR_LEVEL_3,
    COLOR_LEVEL_4,
    COLOR_LEVEL_5
  };

  const JournalSession *session = &s_journal;
  static char start_text[24];
  char end_text[24];
  const char *feeling_text = "";
  const char *achievement_text = "";
  GColor bar_color = GColorLightGray;
  int achievement = journal_top_achievement(session->achievements);
  int feeling = (int) session->feeling;
  int level = (int) session->level;

  if (feeling > 0 && feeling <= 4) {
    feeling_text = tr(STR_EFFORT_RELAXED + (feeling - 1));
  }

  if (achievement >= 0) {
    achievement_text = tr(STR_ACHIEVEMENT_WELCOME_BACK + achievement);
  }

  if (level >= 1 && level <= 5) {
    bar_color = level_colors[level - 1];
  }

  timeline_format_clock(
      (time_t) session->start_time, start_text, sizeof(start_text));

  time_t end_time = s_session_end_time;

  timeline_format_clock(
      session->start_time > 0 ? end_time : 0,
      end_text, sizeof(end_text));

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_fill_color(ctx, bar_color);
  graphics_fill_rect(
      ctx,
      GRect(content_width, 0, bar_width, bounds.size.h),
      0,
      GCornerNone);

  uint8_t event_indices[JOURNAL_MAX_EVENTS];
  int event_count = timeline_event_indices(session, event_indices);
  bool has_achievement = achievement >= 0;
  int start_block = 1 + (has_achievement ? 1 : 0);
  int first_event_block = start_block + 1;
  int block_count = first_event_block + event_count + 1;


  for (int i = 0; i < block_count; i++) {
    bool is_zone_block = i >= first_event_block &&
        i < block_count - 1 &&
        session->events[event_indices[i - first_event_block]].type
            == JE_ZONE;
    int item_height = timeline_block_height(
        i, has_achievement, first_event_block,
        event_count, event_indices);

    int16_t y = first_block_y + timeline_block_offset(i)
        - s_timeline_scroll_offset;

    // Pendant le mouvement, conserve les blocs entrants et sortants.
    if (y + item_height + 16 < 34 || y >= bounds.size.h + 16) {
      continue;
    }

    char event_title[64];
    char zone_range[32];
    char event_time[64];
    char zone_duration_text[64] = "";
    char end_summary_text[64];
    char end_clock_text[64];
    char start_summary_text[64];
    char start_clock_text[64];
    const char *title = "";
    const char *label = "";
    const char *detail = "";

    if (i == 0) {
      title = feeling_text;
      label = tr(STR_SUMMARY_EFFORT);
    } else if (has_achievement && i == 1) {
      title = achievement_text;
      label = tr(STR_SUMMARY_ACHIEVEMENT);
      detail = tr(STR_ACH_DESC_WELCOME_BACK + achievement);
    } else if (i == start_block) {
      title = tr(STR_TIMELINE_START);

      if (s_session_start_bpm > 0) {
        snprintf(
            start_summary_text, sizeof(start_summary_text),
            "00:00 min - %d bpm", s_session_start_bpm);
      } else {
        snprintf(
            start_summary_text, sizeof(start_summary_text),
            "00:00 min - -- bpm");
      }
      label = start_summary_text;

      snprintf(
          start_clock_text, sizeof(start_clock_text), "%s : %s",
          tr(STR_TIMELINE_TIME), start_text);
      detail = start_clock_text;
    } else if (i == block_count - 1) {
      title = tr(STR_TIMELINE_END);

      snprintf(
          end_summary_text, sizeof(end_summary_text),
          "%u:%02u min - %u kcal",
          (unsigned) (session->active_seconds / 60),
          (unsigned) (session->active_seconds % 60),
          (unsigned) session->calories);
      label = end_summary_text;

      snprintf(
          end_clock_text, sizeof(end_clock_text), "%s : %s",
          tr(STR_TIMELINE_TIME), end_text);
      detail = end_clock_text;
    } else {
      int position = i - first_event_block;
      int event_index = event_indices[position];
      const JournalEvent *event = &session->events[event_index];

      snprintf(event_time, sizeof(event_time), "%u:%02u min",
               (unsigned) (event->seconds / 60),
               (unsigned) (event->seconds % 60));
      detail = event_time;

      if (event->type == JE_ZONE) {
        snprintf(event_title, sizeof(event_title), "%u:%02u min - %s",
                 (unsigned) (event->seconds / 60),
                 (unsigned) (event->seconds % 60),
                 tr(STR_WORKOUT_ZONE_1 + event->value));
        title = event_title;

        timeline_format_zone_range(
            event->value, zone_range, sizeof(zone_range));
        label = zone_range;

        uint16_t duration = s_journal_zone_duration[event_index];
        if (duration > 0) {
          snprintf(zone_duration_text, sizeof(zone_duration_text),
                   "%s %u:%02u min", tr(STR_TIMELINE_DURATION),
                   (unsigned) (duration / 60),
                   (unsigned) (duration % 60));
        } else {
          snprintf(zone_duration_text, sizeof(zone_duration_text),
                   "%s --:-- min", tr(STR_TIMELINE_DURATION));
        }
        detail = zone_duration_text;
      } else if (event->type == JE_BORNE) {
        int distance = event->value *
            ((session->activity == ACTIVITY_RUN) ? 2 : 1);
        snprintf(event_title, sizeof(event_title), "%u:%02u min - %d %s",
                 (unsigned) (event->seconds / 60),
                 (unsigned) (event->seconds % 60),
                 distance, s_session_units_imperial ? "mi" : "km");
        title = event_title;
        label = tr(STR_LABEL_DISTANCE);
        detail = "";
      } else {
        snprintf(event_title, sizeof(event_title), "%u:%02u min - %u kcal",
                 (unsigned) (event->seconds / 60),
                 (unsigned) (event->seconds % 60),
                 (unsigned) event->value * 200u);
        title = event_title;
        label = tr(STR_SUMMARY_CALORIES);
        detail = "";
      }
    }

    bool is_endpoint = i == start_block || i == block_count - 1;
    bool is_achievement = has_achievement && i == 1;
    bool is_heading = i == 0 || is_achievement;
    int heading_height = is_heading
        ? timeline_heading_height(is_achievement) : 34;
    GFont title_font = fonts_get_system_font(
        is_achievement ? FONT_KEY_GOTHIC_24_BOLD
                       : FONT_KEY_GOTHIC_28_BOLD);
    GSize title_size = text_small_min_size(
        title,
        title_font,
        GRect(0, 0, 1000, 40),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    int16_t title_y =
        y + (is_heading ? 20 : 0) - s_timeline_stretch / 2;

    if (!is_heading && title_size.w > content_width - 12) {
      title_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
      title_size = text_small_min_size(
          title,
          title_font,
          GRect(0, 0, 1000, 40),
          GTextOverflowModeTrailingEllipsis,
          GTextAlignmentLeft);
    }

    if (!is_heading && title_size.w > content_width - 12) {
      title_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
      title_y += 4;
    }

    if (is_zone_block) {
      const JournalEvent *zone_event =
          &session->events[event_indices[i - first_event_block]];
      const char *zone_text =
          tr(STR_WORKOUT_ZONE_1 + zone_event->value);
      char time_text[16];
      snprintf(time_text, sizeof(time_text), "%u:%02u min",
               (unsigned) (zone_event->seconds / 60),
               (unsigned) (zone_event->seconds % 60));

      title_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
      GSize time_size = text_small_min_size(
          time_text, title_font, GRect(0, 0, 1000, 40),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      GSize zone_size = text_small_min_size(
          zone_text, title_font, GRect(0, 0, 1000, 40),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

      if (time_size.w + 8 + zone_size.w + 12 > content_width - 12) {
        title_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
        time_size = text_small_min_size(
            time_text, title_font, GRect(0, 0, 1000, 40),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
        zone_size = text_small_min_size(
            zone_text, title_font, GRect(0, 0, 1000, 40),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      }

      int16_t pill_x = 6 + time_size.w + 8;
      int16_t pill_width = zone_size.w + 12;
      int16_t available_width = content_width - 6 - pill_x;
      if (pill_width > available_width) {
        pill_width = available_width;
      }

      GColor zone_color = level_colors[zone_event->value];
      int16_t text_y = y - s_timeline_stretch / 2;
      if (title_font == fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)) {
        text_y += 3;
      }
      title_y = text_y;

      graphics_context_set_text_color(ctx, GColorBlack);
      draw_text_small_min(
          ctx, time_text, title_font,
          GRect(6, text_y, time_size.w + 2, 30),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

      graphics_context_set_fill_color(ctx, zone_color);
      graphics_fill_rect(
          ctx, GRect(pill_x, y + 3 - s_timeline_stretch / 2,
                     pill_width, 26),
          8, GCornersAll);
      graphics_context_set_text_color(
          ctx, get_text_color_for_background(zone_color));
      draw_text_small_min(
          ctx, zone_text, title_font,
          GRect(pill_x + 6, text_y, pill_width - 12, 30),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
    } else if (is_achievement) {
      GColor badge_color = ACCENT_YELLOW;
      GColor badge_text_color =
          get_text_color_for_background(badge_color);

      graphics_context_set_fill_color(ctx, badge_color);
      graphics_fill_rect(
          ctx, GRect(6, title_y, content_width - 12, heading_height),
          6, GCornersAll);

      int16_t star_y = title_y + heading_height / 2;
      timeline_draw_small_star(
          ctx, GPoint(17, star_y), badge_text_color);
      timeline_draw_small_star(
          ctx, GPoint(content_width - 17, star_y), badge_text_color);

      graphics_context_set_text_color(ctx, badge_text_color);
      draw_text_small_min(
          ctx, title, title_font,
          GRect(28, title_y + 4, content_width - 56, heading_height - 8),
          GTextOverflowModeWordWrap, GTextAlignmentCenter);
    } else {
      graphics_context_set_text_color(ctx, GColorBlack);
      draw_text_small_min(
          ctx,
          title,
          title_font,
          GRect(6, title_y, content_width - 12, heading_height),
          is_heading ? GTextOverflowModeWordWrap
                     : GTextOverflowModeTrailingEllipsis,
          GTextAlignmentLeft);
    }

    graphics_context_set_text_color(ctx, GColorBlack);

    draw_text_small_min(
        ctx,
        label,
        fonts_get_system_font(
            (is_zone_block || is_endpoint) ? FONT_KEY_GOTHIC_24_BOLD
                                           : FONT_KEY_GOTHIC_18_BOLD),
        GRect(6, y + (is_heading ? 0
                                : (is_endpoint ? 24
                                               : (is_zone_block ? 26 : 28))),
              content_width - 12,
              (is_zone_block || is_endpoint) ? 30 : 20),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);

    if (detail[0] != '\0') {
      GFont detail_font = fonts_get_system_font(
          is_achievement ? FONT_KEY_GOTHIC_24_BOLD
                         : FONT_KEY_GOTHIC_24_BOLD);
      if (!is_achievement) {
        GSize detail_size = text_small_min_size(
            detail, detail_font, GRect(0, 0, 1000, 40),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
        if (detail_size.w > content_width - 12) {
          detail_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
        }
      }

      if (is_endpoint) {
        draw_clock_small_suffix(
            ctx, detail, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
            GRect(6, y + 48 + s_timeline_stretch / 2,
                  content_width - 12, 30),
            GTextAlignmentLeft);
      } else {
        draw_text_small_min(
            ctx,
            detail,
            detail_font,
            GRect(6, y + (is_achievement ? heading_height + 20
                                        : (is_endpoint ? 48
                                                       : (is_zone_block ? 50 : 58)))
                      + s_timeline_stretch / 2,
                  content_width - 12,
                  is_achievement
                      ? item_height - heading_height - 28 : 30),
            is_achievement ? GTextOverflowModeWordWrap
                           : GTextOverflowModeTrailingEllipsis,
            GTextAlignmentLeft);
      }
    }

    TimelineMarker marker;
    if (i == 0) {
      marker = TIMELINE_MARKER_FEELING;
    } else if (has_achievement && i == 1) {
      marker = TIMELINE_MARKER_ACHIEVEMENT;
    } else if (i == start_block) {
      marker = TIMELINE_MARKER_START;
    } else if (i == block_count - 1) {
      marker = TIMELINE_MARKER_END;
    } else {
      int position = i - first_event_block;
      const JournalEvent *event =
          &session->events[event_indices[position]];
      marker = (event->type == JE_ZONE) ? TIMELINE_MARKER_ZONE
          : (event->type == JE_BORNE) ? TIMELINE_MARKER_DISTANCE
          : TIMELINE_MARKER_CALORIES;
    }

    int marker_center_y = title_y + 18;
    if (title_font == fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD)) {
      marker_center_y = title_y + 16;
    } else if (title_font ==
               fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)) {
      marker_center_y = title_y + 12;
    }

    if (is_achievement) {
      marker_center_y = title_y + heading_height / 2;
    }

    timeline_draw_marker(
        ctx,
        GPoint(content_width + bar_width / 2, marker_center_y),
        marker,
        get_text_color_for_background(bar_color));
  }

  graphics_context_set_fill_color(ctx, bar_color);
  graphics_fill_rect(
      ctx,
      GRect(0, 0, bounds.size.w, 34),
      0,
      GCornerNone);

  graphics_context_set_text_color(
      ctx, get_text_color_for_background(bar_color));
  // Renforcement du titre sans changer sa taille ni sa position verticale.
  for (int offset = 0; offset <= 1; offset++) {
    draw_text_small_min(
        ctx,
        tr(STR_TIMELINE_SUMMARY),
        fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(offset, 0, content_width, 28),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter);
  }
}

static void timeline_up_click_handler(
    ClickRecognizerRef recognizer, void *context) {
  if (s_timeline_selection > 0) {
    s_timeline_selection--;
    timeline_scroll_to_selection();
  }
}

static void timeline_down_click_handler(
    ClickRecognizerRef recognizer, void *context) {
  if (s_timeline_selection < s_timeline_page_count - 1) {
    s_timeline_selection++;
    timeline_scroll_to_selection();
  }
}

static void timeline_select_click_handler(
    ClickRecognizerRef recognizer, void *context) {
  timeline_scroll_finish();
  s_summary_from_timeline = true;
  window_stack_push(s_summary_window, true);
}

static void timeline_back_click_handler(
    ClickRecognizerRef recognizer, void *context) {
  // La sortie du parcours passe uniquement par le resume.
}

static void timeline_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, timeline_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, timeline_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, timeline_down_click_handler);
  window_single_click_subscribe(
      BUTTON_ID_SELECT, timeline_select_click_handler);
}

static void timeline_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, GColorWhite);
  s_timeline_selection = 0;
  s_timeline_page_count = 0;
  timeline_scroll_finish();

  s_timeline_layer = layer_create(bounds);
  timeline_build_pages();
  timeline_scroll_finish();
  layer_set_update_proc(s_timeline_layer, timeline_layer_update_proc);
  layer_add_child(window_layer, s_timeline_layer);
}

static void timeline_window_unload(Window *window) {
  timeline_scroll_finish();
  layer_destroy(s_timeline_layer);
  s_timeline_layer = NULL;
}

static void detail_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GRect scroll_frame = GRect(
      0, -s_detail_scroll_offset, bounds.size.w, 800);
  JournalSession *d = &s_detail_session;
  bool is_training = (d->activity == ACTIVITY_TRAINING);
  GFont value_font = s_summary_font;
  int16_t y = scroll_frame.origin.y + 44;
  int16_t step = 72;
  char text[32];
  char date_text[32];
  time_t start_time = (time_t) d->start_time;
  struct tm *start_tm = localtime(&start_time);

  char day_text[12];
  char session_clock_text[16];

  if (start_tm == NULL ||
      strftime(day_text, sizeof(day_text), "%d/%m", start_tm) == 0) {
    snprintf(day_text, sizeof(day_text), "--/--");
  }

  timeline_format_clock(
      start_time, session_clock_text, sizeof(session_clock_text));
  snprintf(date_text, sizeof(date_text), "%s · %s",
           day_text, session_clock_text);

  GFont date_time_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);




  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(
      ctx,
      tr(STR_ACTIVITY_WALK + d->activity),
      s_summary_font,
      GRect(6, scroll_frame.origin.y + 4, bounds.size.w - 12, 44),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft,
      NULL);

  snprintf(
      text, sizeof(text), "%02d:%02d min",
      (int) (d->active_seconds / 60),
      (int) (d->active_seconds % 60));
  draw_detail_line(
      ctx, y, date_text, "", tr(STR_LABEL_DATE),
      date_time_font);
  y += step;

  draw_detail_line(
      ctx, y, text, "", tr(STR_LABEL_DURATION), value_font);
  y += step;

  snprintf(
      text, sizeof(text), "%02d:%02d min",
      (int) (d->total_seconds / 60),
      (int) (d->total_seconds % 60));
  draw_detail_line(
      ctx, y, text, "", tr(STR_SUMMARY_TOTAL_TIME), value_font);
  y += step;

  if (!is_training) {
    uint32_t hundredths =
        (d->distance_m * 100) / 1000;

    snprintf(
        text, sizeof(text), "%lu.%02lu",
        (unsigned long) (hundredths / 100),
        (unsigned long) (hundredths % 100));
    draw_detail_line(
        ctx, y, text, get_distance_unit(),
        tr(STR_LABEL_DISTANCE), value_font);
    y += step;

    snprintf(text, sizeof(text), "%lu",
             (unsigned long) d->steps);
    draw_detail_line(
        ctx, y, text, tr(STR_UNIT_STEPS),
        tr(STR_LABEL_STEPS), value_font);
    y += step;
  }

  if (d->avg_bpm > 0) {
    snprintf(text, sizeof(text), "%d", (int) d->avg_bpm);
    draw_detail_line(
        ctx, y, text, "bpm", tr(STR_SUMMARY_HR_AVG), value_font);
    y += step;
  }

  if (d->max_bpm > 0) {
    snprintf(text, sizeof(text), "%d", (int) d->max_bpm);
    draw_detail_line(
        ctx, y, text, "bpm", tr(STR_SUMMARY_HR_MAX), value_font);
    y += step;
  }

  if (d->calories > 0) {
    snprintf(text, sizeof(text), "%d", (int) d->calories);
    draw_detail_line(
        ctx, y, text, "kcal", tr(STR_SUMMARY_CALORIES), value_font);
    y += step;
  }

  if (d->feeling > 0) {
    draw_detail_line(
        ctx, y,
        tr(STR_EFFORT_RELAXED + (d->feeling - 1)),
        "", tr(STR_SUMMARY_EFFORT), value_font);
    y += step;
  }

  if (d->level > 0) {
    draw_detail_intensity(ctx, y, (int) d->level);
    y += step;
  }

  for (int i = 0; i < JOURNAL_ZONE_COUNT; i++) {
    if (d->zone_seconds[i] > 0) {
      snprintf(
          text, sizeof(text), "%d:%02d min",
          (int) (d->zone_seconds[i] / 60),
          (int) (d->zone_seconds[i] % 60));
      draw_detail_line(
          ctx, y, text, "",
          tr(STR_WORKOUT_ZONE_1 + i), value_font);
      y += step;
    }
  }

  int achievement = journal_top_achievement(d->achievements);

  if (achievement >= 0) {
    draw_detail_line(
        ctx, y,
        tr(STR_ACHIEVEMENT_WELCOME_BACK + achievement),
        "", tr(STR_SUMMARY_ACHIEVEMENT), value_font);
    y += step;
  }

  s_detail_content_height = (y + 12) - scroll_frame.origin.y;
}

static void detail_back_click_handler(
    ClickRecognizerRef recognizer, void *context) {
  detail_cancel_scroll_animation();
  window_stack_pop(true);
}

static void detail_click_config_provider(void *context) {
  window_single_click_subscribe(
      BUTTON_ID_UP, detail_up_click_handler);
  window_single_click_subscribe(
      BUTTON_ID_DOWN, detail_down_click_handler);
  window_single_click_subscribe(
      BUTTON_ID_BACK, detail_back_click_handler);
}

static void detail_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, GColorWhite);
  s_detail_scroll_offset = 0;

  s_detail_layer = layer_create(bounds);
  layer_set_update_proc(s_detail_layer, detail_layer_update_proc);
  layer_add_child(window_layer, s_detail_layer);
}

static void detail_window_unload(Window *window) {
  detail_cancel_scroll_animation();
  layer_destroy(s_detail_layer);
  s_detail_layer = NULL;
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

static void workout_resume_session(void) {
  if (!s_is_paused) {
    return;
  }

  HealthValue total_today = health_service_sum_today(HealthMetricStepCount);
  step_counter_rebase(&s_step_counter, (int) total_today);

  // Nouvelle fenetre d'allure recente, sans effacer les courbes.
  s_graph_distance_count = 0;
  s_is_paused = false;
}

static void workout_up_click_handler(ClickRecognizerRef recognizer,
                                     void *context) {
  if (!s_is_paused) {
    refresh_steps_now();
    s_is_paused = true;

    // Une pause coupe la sequence, sans effacer le maximum.
    s_journal_redline_current = 0;
    journal_zone_pass_reset();
  } else {
    workout_resume_session();
  }

  layer_mark_dirty(s_workout_layer);
}

static void workout_select_click_handler(ClickRecognizerRef recognizer,
                                         void *context) {
  if (s_is_paused) {
    // Heure provisoire de fin, confirmee par le choix du ressenti.
    // Chaque nouvel appui sur Stop remplace la precedente.
    s_session_end_time = time(NULL);
    s_session_total_at_stop =
        (uint32_t) s_elapsed_seconds + (uint32_t) s_paused_seconds;
    window_stack_push(s_stop_confirm_window, true);
  }
}

static bool is_metric_available(WorkoutMetric metric) {
  if (s_selected_activity != ACTIVITY_TRAINING) {
    return true;
  }

  // Entrainement libre : seulement la duree et la frequence cardiaque.
  return metric == METRIC_DURATION ||
         metric == METRIC_HEART_RATE ||
         metric == METRIC_HEART_RATE_GRAPH;
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

static void workout_select_long_click_handler(ClickRecognizerRef recognizer,
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
      workout_select_long_click_handler,
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
  s_session_start_bpm = 0;
  s_heart_rate_available = false;
  s_heart_rate_zone = -1;
  s_seconds_since_last_bpm_read = 0;
  alerts_reset();
  s_current_steps = 0;
  s_seconds_since_last_steps_read = 0;

  HealthValue starting_steps = health_service_sum_today(HealthMetricStepCount);
  step_counter_start(&s_step_counter, (int) starting_steps);
  workout_graph_reset();

  s_bpm_sum = 0;
  s_bpm_sample_count = 0;
  s_zone_points_sum = 0;
  s_effort_choice = 0;
  s_effort_rating = 0;
  s_bpm_max = 0;
  s_paused_seconds = 0;

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
  const int16_t side_bar_width = SIDE_BAR_WIDTH;
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
    COLOR_LEVEL_1,
    COLOR_LEVEL_2,
    COLOR_LEVEL_4,
    COLOR_LEVEL_5
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
        text_color = get_text_color_for_background(effort_colors[i - 1]);
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

// Prepare la seance et les succes en RAM, sans ecriture persistante.
static void journal_session_prepare(void) {
  JournalStats stats;
  JournalDays days;
  int avg_bpm = 0;
  int avg_zone_x10 = 0;
  int score = get_effort_score();
  int level = 0;

  journal_load_stats(&stats);
  journal_load_days(&days);

  if (s_bpm_sample_count > 0) {
    avg_bpm = (int) (s_bpm_sum / s_bpm_sample_count);
    avg_zone_x10 = (int) ((s_zone_points_sum * 10) / s_bpm_sample_count);
  }

  if (score == 0) {
    level = 0;
  } else if (score < 18) {
    level = 1;
  } else if (score < 28) {
    level = 2;
  } else if (score < 38) {
    level = 3;
  } else if (score < 46) {
    level = 4;
  } else {
    level = 5;
  }

  s_journal.active_seconds = (uint16_t) s_elapsed_seconds;
  s_journal.total_seconds = (uint16_t) s_session_total_at_stop;
  s_journal.steps = (uint32_t) s_current_steps;
  s_journal.distance_m = (uint32_t) get_distance_meters();
  s_journal.calories = (uint16_t) get_session_calories();
  s_journal.avg_bpm = (uint8_t) avg_bpm;
  s_journal.max_bpm = (uint8_t) s_bpm_max;
  s_journal.avg_zone_x10 = (uint8_t) avg_zone_x10;
  s_journal.effort_score = (uint8_t) score;
  s_journal.feeling = (uint8_t) s_effort_rating;
  s_journal.level = (uint8_t) level;
  s_journal.achievements = achievements_compute(
      &s_journal, &stats, &days, &s_goals,
      s_session_start_hour, s_session_start_day);
  s_session_pending_save =
      s_elapsed_seconds >= MINIMUM_SESSION_SECONDS;

}

static void stop_confirm_select_click_handler(ClickRecognizerRef recognizer,
                                              void *context) {
  if (s_effort_choice == 0) {
    workout_resume_session();
    layer_mark_dirty(s_workout_layer);
    window_stack_pop(true);
    return;
  }

  s_effort_rating = s_effort_choice;
  tick_timer_service_unsubscribe();
  s_summary_from_timeline = false;

  if (s_elapsed_seconds >= MINIMUM_SESSION_SECONDS) {
    journal_session_prepare();
  }

  window_stack_pop(true);
  window_stack_pop(true);

  if (s_elapsed_seconds < MINIMUM_SESSION_SECONDS) {
    window_stack_push(s_summary_window, true);
  } else {
    window_stack_push(s_timeline_window, true);
  }
}

static void stop_confirm_back_click_handler(ClickRecognizerRef recognizer,
                                            void *context) {
  workout_resume_session();
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
static GColor s_summary_value_color;

static GColor get_label_color_for_background(GColor background) {
  if (gcolor_equal(background, COLOR_LEVEL_5)) {
    return GColorWhite;
  }

  if (gcolor_equal(background, COLOR_LEVEL_1) ||
      gcolor_equal(background, COLOR_LEVEL_4)) {
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

  graphics_context_set_text_color(ctx, s_summary_label_color);
  graphics_draw_text(
      ctx,
      label,
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(6, y, 188, 22),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft,
      NULL);

  graphics_context_set_text_color(ctx, s_summary_value_color);
  graphics_draw_text(
      ctx,
      value,
      value_font,
      GRect(6, y + 22, 188, 44),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft,
      NULL);

  if (unit[0] != '\0') {
    graphics_draw_text(
        ctx,
        unit,
        fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
        GRect(6 + value_size.w + 6, y + 22 + SUMMARY_UNIT_Y_OFFSET, 110, 34),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft,
        NULL);
  }
}

// Score d'effort de 10 a 50 : FC moyenne (zone x 10) et effort ressenti.
// Renvoie 0 si aucune donnee exploitable.
static void draw_summary_separator(GContext *ctx, int16_t y) {
  graphics_context_set_stroke_color(ctx, s_summary_value_color);
  graphics_draw_line(ctx, GPoint(6, y + 70), GPoint(194, y + 70));
}

static void draw_summary_intensity(GContext *ctx, int16_t y, int level) {
  const int16_t segment_width = 30;
  const int16_t segment_height = 20;
  const int16_t segment_gap = 4;
  const int16_t segment_y = y + 26;

  for (int i = 0; i < 5; i++) {
    int16_t x = 6 + i * (segment_width + segment_gap);

    if (i < level) {
      graphics_context_set_fill_color(ctx, s_summary_value_color);
      graphics_fill_rect(
          ctx,
          GRect(x, segment_y, segment_width, segment_height),
          2,
          GCornersAll);
    } else {
      graphics_context_set_stroke_color(ctx, s_summary_value_color);
      graphics_draw_round_rect(
          ctx,
          GRect(x, segment_y, segment_width, segment_height),
          2);
    }
  }

  graphics_context_set_text_color(ctx, s_summary_label_color);
  graphics_draw_text(
      ctx,
      tr(STR_SUMMARY_INTENSITY),
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
      GRect(6, y, 188, 22),
      GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft,
      NULL);

  draw_summary_separator(ctx, y);
}

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
    return COLOR_LEVEL_1;
  } else if (score < 28) {
    return COLOR_LEVEL_2;
  } else if (score < 38) {
    return COLOR_LEVEL_3;
  } else if (score < 46) {
    return COLOR_LEVEL_4;
  }

  return COLOR_LEVEL_5;
}

static void summary_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  GColor background = (s_elapsed_seconds < MINIMUM_SESSION_SECONDS)
      ? ACCENT_YELLOW
      : get_effort_color();
  s_summary_label_color = get_label_color_for_background(background);
  s_summary_value_color = get_text_color_for_background(background);
  graphics_context_set_fill_color(ctx, background);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (s_elapsed_seconds < MINIMUM_SESSION_SECONDS) {
    int minutes = s_elapsed_seconds / 60;
    int seconds = s_elapsed_seconds % 60;
    char duration_text[16];

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

  char duration_text[16];
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
      get_distance_hundredths(distance_meters) / 100,
      get_distance_hundredths(distance_meters) % 100);

  if (is_pace_displayable(distance_meters)) {
    long pace_seconds_per_km =
        get_pace_seconds(distance_meters);

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
        get_speed_hundredths(distance_meters);

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

  graphics_context_set_text_color(ctx, s_summary_value_color);
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
  int achievement = journal_top_achievement(s_journal.achievements);

  if (achievement >= 0) {
    draw_summary_line(
        ctx, y,
        tr(STR_ACHIEVEMENT_WELCOME_BACK + achievement),
        "", tr(STR_SUMMARY_ACHIEVEMENT), value_font);
    draw_summary_separator(ctx, y);
    y += step;
  }

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
    draw_summary_separator(ctx, y);
    y += step;
  }

  draw_summary_intensity(ctx, y, (int) s_journal.level);
  y += step;

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
  draw_summary_separator(ctx, y);
  y += step;

  if (s_selected_activity != ACTIVITY_TRAINING) {
    draw_summary_line(ctx, y, steps_text, tr(STR_UNIT_STEPS), tr(STR_LABEL_STEPS), value_font);
    draw_summary_separator(ctx, y);
    y += step;

    draw_summary_line(ctx, y, distance_text, get_distance_unit(), tr(STR_LABEL_DISTANCE), value_font);
    draw_summary_separator(ctx, y);
    y += step;

    draw_summary_line(ctx, y, pace_text, get_pace_unit(), tr(STR_SUMMARY_PACE), value_font);
    draw_summary_separator(ctx, y);
    y += step;

    draw_summary_line(ctx, y, speed_text, get_speed_unit(), tr(STR_SUMMARY_SPEED), value_font);
    draw_summary_separator(ctx, y);
    y += step;
  }

  if (!(hide_unmeasured && average_bpm <= 0)) {
    draw_summary_line(
        ctx, y, bpm_avg_text, bpm_avg_unit, tr(STR_SUMMARY_HR_AVG), bpm_font_avg);
    draw_summary_separator(ctx, y);
    y += step;
  }

  if (!(hide_unmeasured && s_bpm_max <= 0)) {
    draw_summary_line(
        ctx, y, bpm_max_text, bpm_max_unit, tr(STR_SUMMARY_HR_MAX), bpm_font_max);
    draw_summary_separator(ctx, y);
    y += step;
  }

  if (!(hide_unmeasured && calories <= 0)) {
    draw_summary_line(ctx, y, calories_text, "kcal", tr(STR_SUMMARY_CALORIES), value_font);
    draw_summary_separator(ctx, y);
    y += step;
  }

  s_summary_content_height = (y + 12) - scroll_frame.origin.y;
}

static void summary_up_click_handler(ClickRecognizerRef recognizer,
                                     void *context) {
  if (s_elapsed_seconds < MINIMUM_SESSION_SECONDS) {
    session_return_to_menu();
    return;
  }

  s_summary_scroll_offset -= SUMMARY_SCROLL_STEP;

  if (s_summary_scroll_offset < 0) {
    s_summary_scroll_offset = 0;
  }

  layer_mark_dirty(s_summary_layer);
}

static void summary_down_click_handler(ClickRecognizerRef recognizer,
                                       void *context) {
  if (s_elapsed_seconds < MINIMUM_SESSION_SECONDS) {
    session_return_to_menu();
    return;
  }

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

static void session_return_to_menu(void) {
  s_session_pending_save = false;
  s_summary_from_timeline = false;

  while (window_stack_get_top_window() != NULL &&
         window_stack_get_top_window() != s_menu_window) {
    window_stack_pop(true);
  }
}

static void save_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int16_t bar_width = SIDE_BAR_WIDTH;
  const int16_t content_width = bounds.size.w - bar_width;
  const int16_t center_x = content_width + bar_width / 2;
  GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont message_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(
      ctx, GRect(content_width, 0, bar_width, bounds.size.h),
      0, GCornerNone);

  // Disquette blanche, centree face au bouton Haut.
  int16_t x = center_x - 11;
  int16_t y = EFFORT_ICON_UP_CENTER_Y - 11;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(x, y, 22, 22), 2, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(x + 12, y + 2, 6, 6), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 4, y + 13, 14, 7), 0, GCornerNone);

  // Poubelle blanche, centree face au bouton Bas.
  y = EFFORT_ICON_DOWN_CENTER_Y - 11;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(x + 8, y, 6, 3), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 2, y + 4, 18, 3), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 4, y + 9, 14, 14), 2, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(x + 7, y + 11, 2, 9), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 13, y + 11, 2, 9), 0, GCornerNone);

  const char *warning = s_save_error ? tr(STR_SAVE_ERROR)
      : (s_save_full ? tr(STR_SAVE_REPLACE_WARNING) : "");
  const char *title = tr(STR_SAVE_QUESTION);
  const int16_t text_width = content_width - 16;
  const int16_t gap = warning[0] != '\0' ? 12 : 0;

  GSize title_size = graphics_text_layout_get_content_size(
      title, title_font, GRect(0, 0, text_width, 1000),
      GTextOverflowModeWordWrap, GTextAlignmentCenter);

  GSize message_size = GSize(0, 0);
  if (warning[0] != '\0') {
    message_size = graphics_text_layout_get_content_size(
        warning, message_font, GRect(0, 0, text_width, 1000),
        GTextOverflowModeWordWrap, GTextAlignmentCenter);
  }

  int16_t title_height = title_size.h + 4;
  int16_t message_height =
      warning[0] != '\0' ? message_size.h + 4 : 0;

  const int16_t title_y = warning[0] != '\0'
      ? 16
      : (bounds.size.h - title_height) / 2;
  const int16_t message_top = title_y + title_height + gap;
  const int16_t message_bottom = bounds.size.h - 16;
  const int16_t available_height = message_bottom - message_top;

  if (warning[0] != '\0' && message_height > available_height) {
    message_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    message_size = graphics_text_layout_get_content_size(
        warning, message_font, GRect(0, 0, text_width, 1000),
        GTextOverflowModeWordWrap, GTextAlignmentCenter);
    message_height = message_size.h + 4;
  }

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(
      ctx, title, title_font,
      GRect(8, title_y, text_width, title_height),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  if (warning[0] != '\0') {
    // Centre le message entre les icones Haut et Bas.
    const int16_t icons_center_y =
        (EFFORT_ICON_UP_CENTER_Y + EFFORT_ICON_DOWN_CENTER_Y) / 2;
    int16_t message_y = icons_center_y - message_height / 2;
    if (message_y < message_top) {
      message_y = message_top;
    }

    graphics_draw_text(
        ctx, warning, message_font,
        GRect(8, message_y, text_width, message_bottom - message_y),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }

}

static void save_up_click_handler(
    ClickRecognizerRef recognizer, void *context) {
  if (!s_session_pending_save ||
      s_elapsed_seconds < MINIMUM_SESSION_SECONDS ||
      s_save_error) {
    return;
  }

  if (journal_save_all(&s_journal, s_session_start_day)) {
    session_return_to_menu();
  } else {
    // Pas de nouvelle tentative apres une eventuelle ecriture partielle.
    s_save_error = true;
    layer_mark_dirty(s_save_layer);
  }
}

static void save_down_click_handler(
    ClickRecognizerRef recognizer, void *context) {
  // Abandonne la seance courante, sans supprimer une ancienne seance.
  session_return_to_menu();
}

static void save_noop_click_handler(
    ClickRecognizerRef recognizer, void *context) {
}

static void save_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, save_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, save_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, save_noop_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, save_noop_click_handler);
}

static void save_window_load(Window *window) {
  JournalIndex index;
  journal_load_index(&index);
  s_save_full = journal_index_is_full(&index);
  s_save_error = false;

  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorWhite);
  s_save_layer = layer_create(bounds);
  layer_set_update_proc(s_save_layer, save_layer_update_proc);
  layer_add_child(root, s_save_layer);
}

static void save_window_unload(Window *window) {
  layer_destroy(s_save_layer);
  s_save_layer = NULL;
}

static void summary_select_click_handler(ClickRecognizerRef recognizer,
                                         void *context) {
  if (s_elapsed_seconds < MINIMUM_SESSION_SECONDS ||
      !s_session_pending_save) {
    session_return_to_menu();
    return;
  }

  window_stack_push(s_save_window, true);
}

static void summary_back_click_handler(ClickRecognizerRef recognizer,
                                       void *context) {
  if (s_elapsed_seconds < MINIMUM_SESSION_SECONDS) {
    session_return_to_menu();
    return;
  }

  if (s_summary_from_timeline) {
    s_summary_from_timeline = false;
    window_stack_pop(true);
    return;
  }

  summary_select_click_handler(recognizer, context);
}

static void summary_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, summary_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, summary_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, summary_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, summary_back_click_handler);
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
  if (s_profile.resting_hr != 0 &&
      (s_profile.resting_hr < 35 || s_profile.resting_hr > 110)) {
    s_profile.resting_hr = 0;
  }
}

// Conversions imperial -> metrique, en entiers (arrondi au plus proche).
// 1 pouce = 2,54 cm ; 1 livre = 0,45359 kg.
static int inches_to_cm(int inches) {
  return (inches * 254 + 50) / 100;
}

static int pounds_to_kg(int pounds) {
  return (int) (((int64_t) pounds * 45359 + 50000) / 100000);
}

static void stride_load(void) {
  if (persist_exists(STRIDE_PERSIST_KEY)) {
    persist_read_data(STRIDE_PERSIST_KEY, &s_stride, sizeof(s_stride));
  }

  s_stride.units_imperial = s_stride.units_imperial ? 1 : 0;
  s_stride.walk_enabled = s_stride.walk_enabled ? 1 : 0;
  s_stride.run_enabled = s_stride.run_enabled ? 1 : 0;
}

static void stride_save(void) {
  persist_write_data(STRIDE_PERSIST_KEY, &s_stride, sizeof(s_stride));
}

static void clock_load(void) {
  if (persist_exists(CLOCK_PERSIST_KEY)) {
    persist_read_data(CLOCK_PERSIST_KEY, &s_clock, sizeof(s_clock));
  }

  s_clock.use_12h = s_clock.use_12h ? 1 : 0;
  s_clock.show_ampm = s_clock.show_ampm ? 1 : 0;
  s_clock.hide_hour_zero = s_clock.hide_hour_zero ? 1 : 0;
}

static void clock_save(void) {
  persist_write_data(CLOCK_PERSIST_KEY, &s_clock, sizeof(s_clock));
}

static void alerts_load(void) {
  if (persist_exists(ALERT_PERSIST_KEY)) {
    int stored_size = persist_get_size(ALERT_PERSIST_KEY);
    // Ancien format : sept int16_t ; nouveau : huit.
    if (stored_size == 7 * (int) sizeof(int16_t) ||
        stored_size == (int) sizeof(s_alerts)) {
      persist_read_data(ALERT_PERSIST_KEY, &s_alerts, sizeof(s_alerts));
    }
  }

  if (s_alerts.vibration_level < 0 || s_alerts.vibration_level > 2) {
    s_alerts.vibration_level = 1;
  }

  s_alerts.vibrate_km = s_alerts.vibrate_km ? 1 : 0;
  s_alerts.vibrate_zone = s_alerts.vibrate_zone ? 1 : 0;
  s_alerts.sound_km = s_alerts.sound_km ? 1 : 0;
  s_alerts.sound_zone = s_alerts.sound_zone ? 1 : 0;
  s_alerts.vibrate_interval = s_alerts.vibrate_interval ? 1 : 0;
  s_alerts.sound_interval = s_alerts.sound_interval ? 1 : 0;

  switch (s_alerts.interval_minutes) {
    case 2: case 5: case 10: case 15: case 20: case 30: case 60:
      break;
    default:
      s_alerts.interval_minutes = 0;
  }
}

static void alerts_save(void) {
  persist_write_data(ALERT_PERSIST_KEY, &s_alerts, sizeof(s_alerts));
}

static void goals_load(void);

static void profile_load(void) {
  if (persist_exists(PROFILE_PERSIST_KEY)) {
    persist_read_data(PROFILE_PERSIST_KEY, &s_profile, sizeof(s_profile));
  }

  profile_sanitize();
  stride_load();
  clock_load();
  alerts_load();
  goals_load();
}

static void goals_load(void) {
  if (persist_exists(JOURNAL_GOALS_PERSIST_KEY)) {
    persist_read_data(JOURNAL_GOALS_PERSIST_KEY, &s_goals,
                      sizeof(s_goals));
  }

  if (s_goals.week_distance_m == 0) {
    s_goals.week_distance_m = 50000;
  }
  s_goals.goal_is_steps = s_goals.goal_is_steps ? 1 : 0;
}

static void goals_save(void) {
  persist_write_data(JOURNAL_GOALS_PERSIST_KEY, &s_goals,
                     sizeof(s_goals));
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

  tuple = dict_find(iter, MESSAGE_KEY_RestingHrOn);
  if (tuple) {
    Tuple *rest = dict_find(iter, MESSAGE_KEY_RestingHr);

    s_profile.resting_hr = (tuple_to_int(tuple) == 1 && rest)
        ? (int16_t) tuple_to_int(rest) : 0;
  }

  int imperial = s_stride.units_imperial;

  tuple = dict_find(iter, MESSAGE_KEY_Units);
  if (tuple) {
    imperial = (tuple_to_int(tuple) == 1) ? 1 : 0;
    s_stride.units_imperial = imperial;
  }

  // En imperial, taille (pieds + pouces) et poids (livres) remplacent les
  // valeurs metriques lues plus haut (curseurs masques sur la page).
  if (imperial) {
    Tuple *feet = dict_find(iter, MESSAGE_KEY_HeightFt);
    Tuple *inches = dict_find(iter, MESSAGE_KEY_HeightIn);
    Tuple *pounds = dict_find(iter, MESSAGE_KEY_WeightLb);

    if (feet && inches) {
      s_profile.height_cm = (int16_t) inches_to_cm(
          tuple_to_int(feet) * 12 + tuple_to_int(inches));
    }

    if (pounds) {
      s_profile.weight_kg = (int16_t) pounds_to_kg(tuple_to_int(pounds));
    }
  }

  tuple = dict_find(iter, MESSAGE_KEY_StrideWalkOn);
  if (tuple) {
    s_stride.walk_enabled = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, imperial ? MESSAGE_KEY_StrideWalkInch : MESSAGE_KEY_StrideWalk);
  if (tuple) {
    int value = tuple_to_int(tuple);
    s_stride.walk_cm = (int16_t) (imperial ? inches_to_cm(value) : value);
  }

  tuple = dict_find(iter, MESSAGE_KEY_StrideRunOn);
  if (tuple) {
    s_stride.run_enabled = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, imperial ? MESSAGE_KEY_StrideRunInch : MESSAGE_KEY_StrideRun);
  if (tuple) {
    int value = tuple_to_int(tuple);
    s_stride.run_cm = (int16_t) (imperial ? inches_to_cm(value) : value);
  }

  tuple = dict_find(iter, MESSAGE_KEY_Clock12h);
  if (tuple) {
    s_clock.use_12h = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_ShowAmPm);
  if (tuple) {
    s_clock.show_ampm = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_HideHourZero);
  if (tuple) {
    s_clock.hide_hour_zero = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_VibrateKm);
  if (tuple) {
    s_alerts.vibrate_km = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_VibrateZone);
  if (tuple) {
    s_alerts.vibrate_zone = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_SoundKm);
  if (tuple) {
    s_alerts.sound_km = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_SoundZone);
  if (tuple) {
    s_alerts.sound_zone = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_IntervalMin);
  if (tuple) {
    s_alerts.interval_minutes = (int16_t) tuple_to_int(tuple);
  }

  tuple = dict_find(iter, MESSAGE_KEY_VibrateInterval);
  if (tuple) {
    s_alerts.vibrate_interval = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_SoundInterval);
  if (tuple) {
    s_alerts.sound_interval = (tuple_to_int(tuple) == 1) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_VibrationLevel);
  if (tuple) {
    int level = tuple_to_int(tuple);
    s_alerts.vibration_level = (level >= 0 && level <= 2) ? level : 1;
  }

  tuple = dict_find(iter, MESSAGE_KEY_GoalSessionSteps);
  if (tuple) {
    s_goals.session_goal = (uint32_t) tuple_to_int(tuple);
    s_goals.goal_is_steps = (s_goals.session_goal > 0) ? 1 : 0;
  }

  tuple = dict_find(iter, MESSAGE_KEY_GoalSessionDistanceKm);
  if (tuple) {
    int km = tuple_to_int(tuple);

    if (s_goals.goal_is_steps == 0) {
      s_goals.session_goal =
          (km > 0 && km <= 10000) ? (uint32_t) km * 1000 : 0;
    }
  }

  tuple = dict_find(iter, MESSAGE_KEY_GoalWeekDistanceKm);
  if (tuple) {
    int km = tuple_to_int(tuple);

    s_goals.week_distance_m =
        (km > 0 && km <= 10000) ? (uint32_t) km * 1000 : 50000;
  }

  tuple = dict_find(iter, MESSAGE_KEY_GoalWeekSessions);
  if (tuple) {
    s_goals.week_sessions = (uint16_t) tuple_to_int(tuple);
  }

  alerts_save();
  clock_save();
  goals_save();

  profile_sanitize();
  profile_save();
  stride_save();

}

static void init(void) {
  profile_load();
  app_message_register_inbox_received(profile_inbox_received_handler);
  app_message_open(512, 32);

  s_roboto_condensed_extrabold_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_EXTRABOLD_56));

  s_roboto_condensed_extrabold_countdown_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_EXTRABOLD_100));

  s_clock_font = fonts_load_custom_font(
      resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_EXTRABOLD_64));

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

  s_journal_window = window_create();
  window_set_click_config_provider(
      s_journal_window,
      journal_click_config_provider);
  window_set_window_handlers(
      s_journal_window,
      (WindowHandlers) {
          .load = journal_window_load,
          .unload = journal_window_unload,
      });

  s_detail_window = window_create();
  window_set_click_config_provider(
      s_detail_window,
      detail_click_config_provider);
  window_set_window_handlers(
      s_detail_window,
      (WindowHandlers) {
          .load = detail_window_load,
          .unload = detail_window_unload,
      });

  s_timeline_window = window_create();
  window_set_click_config_provider(
      s_timeline_window,
      timeline_click_config_provider);
  window_set_window_handlers(
      s_timeline_window,
      (WindowHandlers) {
          .load = timeline_window_load,
          .unload = timeline_window_unload,
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

  s_save_window = window_create();
  window_set_click_config_provider(
      s_save_window, save_click_config_provider);
  window_set_window_handlers(
      s_save_window,
      (WindowHandlers) {
          .load = save_window_load,
          .unload = save_window_unload,
      });

  window_stack_push(s_menu_window, true);
}

static void deinit(void) {
  if (s_countdown_timer != NULL) {
    app_timer_cancel(s_countdown_timer);
    s_countdown_timer = NULL;
  }

  tick_timer_service_unsubscribe();

  window_destroy(s_save_window);
  window_destroy(s_summary_window);
  window_destroy(s_timeline_window);
  window_destroy(s_detail_window);
  window_destroy(s_stop_confirm_window);
  window_destroy(s_workout_window);
  window_destroy(s_countdown_window);
  window_destroy(s_journal_window);
  window_destroy(s_menu_window);

  fonts_unload_custom_font(s_roboto_condensed_extrabold_font);
  fonts_unload_custom_font(s_roboto_condensed_extrabold_countdown_font);
  fonts_unload_custom_font(s_clock_font);
  fonts_unload_custom_font(s_summary_font);
  fonts_unload_custom_font(s_emoji_font);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
