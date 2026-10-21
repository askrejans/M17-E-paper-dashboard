// Native (host) unit test runner:  pio test -e native
#include <unity.h>
#include <stdlib.h>
#include "font_8x16.h"
#include "settings.h"
#include "lang.h"
#include "config.h"

unsigned char *fb;

#define TESTS(X)                                                                                                       \
  /* utils */                                                                                                          \
  X(test_parse_date_formats) X(test_parse_date_rejects_garbage) X(test_format_date_all_targets) X(test_day_of_week)    \
  X(test_extract_hhmm) X(test_fmt_num) X(test_utf8_len_and_clip) X(test_utf8_upper_latvian) X(test_parse_number)       \
  X(test_alerts) X(test_power_factor_normalisation) X(test_legacy_bad_temp_hum) X(test_quiet_hours)                    \
  X(test_seconds_to_slot)                                                                                              \
  /* settings */                                                                                                       \
  X(test_settings_defaults) X(test_settings_roundtrip) X(test_settings_masks_secrets) X(test_settings_partial_update)   \
  X(test_settings_validation) X(test_settings_binding_limits) X(test_widget_and_theme_registry)                        \
  /* payload */                                                                                                        \
  X(test_payload_v2_full) X(test_payload_v1_legacy_rooms) X(test_payload_missing_and_unavailable)                      \
  X(test_payload_capacity_limits) X(test_payload_long_strings_truncated) X(test_payload_utf8_strings_truncated) X(test_payload_prices_variants)               \
  X(test_payload_forecast_native_ha_names) X(test_ha_forecast_skips_today) X(test_ha_hourly_list)                      \
  X(test_data_to_json_roundtrip) X(test_jf_number_parsing)                                                             \
  /* home assistant template */                                                                                        \
  X(test_valid_entity_id) X(test_template_uses_mapping) X(test_template_invalid_entities_become_null)                  \
  X(test_template_auto_discovery_toggle) X(test_template_normal_openings_mapping) X(test_entity_search_template)       \
  /* language */                                                                                                       \
  X(test_lang_all_strings_present) X(test_lang_switching) X(test_translate_weather_states)                             \
  X(test_translate_family_states) X(test_translate_names)                                                              \
  /* drawing */                                                                                                        \
  X(test_px_packing) X(test_px_clipping) X(test_fill_all_colours) X(test_fill_rect_lines_and_rects)                    \
  X(test_dither_patterns) X(test_gradient_fades_left_to_right) X(test_line_and_circle) X(test_dotted_lines)            \
  X(test_utf8_decoding) X(test_text_rendering) X(test_seven_segment) X(test_icons) X(test_bmp_export)                  \
  /* fonts */                                                                                                          \
  X(test_font_ascii_preserved) X(test_font_latvian_glyphs_present) X(test_font_latvian_metrics)                        \
  X(test_font_macron_capitals_full_height) X(test_font_symbols)                                                        \
  /* widgets */                                                                                                        \
  X(test_widget_all_ids_build) X(test_widget_outdoor) X(test_widget_indoor_and_bath_rule)                              \
  X(test_widget_forecast_weekdays) X(test_widget_family) X(test_widget_power_and_prices)                               \
  X(test_widget_hourly_house_calendar) X(test_widget_empty_data) X(test_status_flags) X(test_weekday_names) X(test_widget_home)            \
  /* themes */                                                                                                         \
  X(test_themes_render_sample_all_languages) X(test_themes_background_identity) X(test_themes_deterministic)           \
  X(test_themes_every_widget_in_every_slot) X(test_themes_empty_and_v1_data) X(test_theme_slot_mapping_changes_output) \
  X(test_theme_unknown_falls_back_to_classic) X(test_status_screens) X(test_themes_banded_render_matches_full)

#define DECLARE(name) void name(void);
TESTS(DECLARE)

void setUp(void) { setLanguage("EN"); }
void tearDown(void) {}

int main(int, char **)
{
  fb = (unsigned char *)malloc(EPD_WIDTH * EPD_HEIGHT / 4);
  font8x16_init();
  settingsDefaults(settings);
  setLanguage("EN");
  UNITY_BEGIN();
#define RUN(name) RUN_TEST(name);
  TESTS(RUN)
  int r = UNITY_END();
  free(fb);
  return r;
}
