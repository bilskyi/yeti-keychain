# Host-side tests and face previews. Builds the pure-C parts of the firmware
# (graphics, face, mood, power policy, battery maths, button and motion logic) with the Mac's clang.
#
#   make test      run the unit tests
#   make preview   render every face to out/faces.png
#   make analyze   clang static analyzer over the same sources

CC      ?= cc
CFLAGS  := -std=c11 -O1 -g -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes \
           -Wmissing-prototypes -I../main -fsanitize=address,undefined
SRCS    := ../main/display/gfx.c ../main/face/face.c ../main/face/mood.c ../main/face/hud.c \
           ../main/input/input_event.c ../main/power/policy.c \
           ../main/power/battery_logic.c ../main/input/button_logic.c \
           ../main/input/motion_logic.c ../main/display/font.c ../main/app/app.c \
           ../main/app/game_luck.c ../main/app/game_reaction.c ../main/app/game_catch.c \
           ../main/app/game_flappy.c ../main/app/game_snake.c ../main/app/game_memory.c \
           ../main/app/care.c ../main/pet/pet.c ../main/input/orient.c
OUT     := out

.PHONY: test preview analyze clean
test: $(OUT)/face_test $(OUT)/input_test $(OUT)/app_test
	./$(OUT)/face_test
	./$(OUT)/input_test
	./$(OUT)/app_test

preview: $(OUT)/face_preview
	./$(OUT)/face_preview $(OUT)
	python3 -I png_sheet.py $(OUT) $(OUT)/faces.png

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
