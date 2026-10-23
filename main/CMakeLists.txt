# Sources are globbed per folder so adding a file to display/, face/, input/,
# power/ or net/ needs no edit here. Each feature's code is always compiled and
# guarded inside by its FEATURE_* flag, so a flag can never "forget" a file.
file(GLOB KC_SRCS
     "*.c" "display/*.c" "face/*.c" "input/*.c" "power/*.c" "net/*.c")

idf_component_register(SRCS ${KC_SRCS}
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_i2c esp_driver_gpio esp_adc
                                     esp_pm esp_timer)

# Stricter than IDF's defaults, for our code only, and fatal: v0.1 ships with
# zero warnings and stays that way.
target_compile_options(${COMPONENT_LIB} PRIVATE
                       -Wall -Wextra -Werror -Wshadow -Wformat=2
                       -Wstrict-prototypes -Wmissing-prototypes)

# Feature-flag overrides, e.g. -DKC_FEATURES="FEATURE_BUTTONS=1;FEATURE_ACCEL=1"
if(KC_FEATURES)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ${KC_FEATURES})
endif()

# Static analysis pass: idf.py -B build-analyze -DKC_ANALYZE=1 build
if(KC_ANALYZE)
    target_compile_options(${COMPONENT_LIB} PRIVATE -fanalyzer)
endif()
