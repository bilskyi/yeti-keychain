# Listed explicitly: IDF evaluates this file in script mode, where a glob
# cannot re-run on its own, so a new file would silently not be built.
# Feature-specific files are always listed; each is guarded inside by its
# FEATURE_* flag, so flipping a flag never needs an edit here.
set(KC_SRCS
    main.c
    i2c_bus.c
    display/gfx.c
    display/display.c
    face/face.c
    face/mood.c
    face/hud.c
    input/input_event.c
    input/input.c
    input/buttons.c
    input/accel_lis3dh.c
    power/power.c
    power/policy.c
    power/battery.c
    power/battery_logic.c
    net/net.c)

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
