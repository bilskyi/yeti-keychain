# Host-side tests and face previews. Builds the pure-C parts of the firmware
# (graphics, face, mood, power policy, battery maths, button and motion logic) with the Mac's clang.
#
#   make test      run the unit tests
#   make preview   render every face to out/faces.png, the menu and games to out/app.png
#   make analyze   clang static analyzer over the same sources

CC      ?= cc
CFLAGS  := -std=c11 -O1 -g -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes \
           -Wmissing-prototypes -I../main -fsanitize=address,undefined
SRCS    := ../main/display/gfx.c ../main/face/face.c ../main/face/mood.c ../main/face/hud.c \
           ../main/input/input_event.c ../main/power/policy.c \
           ../main/power/battery_logic.c ../main/input/button_logic.c \
           ../main/input/motion_logic.c ../main/display/font.c ../main/app/app.c \
           ../main/app/game_dice.c ../main/app/game_reaction.c ../main/app/game_catch.c ../main/app/game_maze.c \
           ../main/app/game_flappy.c ../main/app/game_snake.c ../main/app/game_voice.c \
           ../main/app/game_setup.c ../main/input/orient.c \
           ../main/app/game_music.c ../main/app/music_viz.c ../main/app/game_egg.c ../main/app/art.c ../main/pet/story.c ../main/sound/rtttl.c \
           ../main/input/touch_logic.c ../main/input/mic_logic.c \
           ../main/link/bitlink.c ../main/link/pong.c ../main/app/game_toss.c ../main/app/game_pong.c ../main/app/game_update.c
OUT     := out

.PHONY: test preview analyze clean
test: $(OUT)/face_test $(OUT)/input_test $(OUT)/app_test
	./$(OUT)/face_test
	./$(OUT)/input_test
	./$(OUT)/app_test

preview: $(OUT)/face_preview $(OUT)/app_preview
	rm -rf $(OUT)/faces $(OUT)/app && mkdir -p $(OUT)/faces $(OUT)/app
	./$(OUT)/face_preview $(OUT)/faces
	python3 -I png_sheet.py $(OUT)/faces $(OUT)/faces.png
	./$(OUT)/app_preview $(OUT)/app
	python3 -I png_sheet.py $(OUT)/app $(OUT)/app.png

analyze:
	clang --analyze -Xanalyzer -analyzer-output=text -std=c11 -I../main \
	      -Xclang -analyzer-werror $(SRCS)
	rm -f *.plist

$(OUT)/%: %.c $(SRCS) $(SRCS) $(wildcard ../main/*/*.h ../main/*.h) | $(OUT)
	$(CC) $(CFLAGS) -o $@ $< $(SRCS)

$(OUT):
	mkdir -p $(OUT)

clean:
	rm -rf $(OUT)
