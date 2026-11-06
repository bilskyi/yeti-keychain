# PROGRESS (source of truth, re-read at the start of every session)

## Decisions made
- 2026-10-23 **Toolchain: ESP-IDF v5.3.2 (C), not PlatformIO/Arduino.** Sasha
  asked not to download PlatformIO; ESP-IDF + the riscv toolchain were already
  installed (`~/esp/esp-idf`), same as the BIT project. Consequences:
  - no U8g2: own SH1106/SSD1306 driver + small graphics lib (`main/display/`);
  - no external libraries, so nothing to pin; IDF version pinned by install;
  - `pio check` (cppcheck) replaced by GCC `-fanalyzer` on the firmware
    (`-DKC_ANALYZE=1`, warnings fatal) + `clang --analyze` on host-built logic;
  - sources live in `main/` (IDF convention) instead of `src/`.
- Use the py3.12 IDF Python env (`. ./env.sh`); the py3.14 env fails export.sh's
  requirements check (esp-coredump 1.17.0 excluded by constraints).
- Our component builds with `-Wall -Wextra -Werror` + extras.
- Feature flags overridable with `-DKC_FEATURES="FEATURE_X=1;..."`, one build
  dir per variant (`-B build-allon`). Sources listed explicitly in
  `main/CMakeLists.txt` (IDF can't re-glob).
- Sleep = 10 min without input (was an uptime cycle until v0.7); any input
  or picking it up wakes it; face down turns the screen off. Sleeping face =
  shut eyes + zzz (~130 lit px vs ~1400 awake).
- Low battery / brownout: low-battery face 4 s, then deep sleep with a 10 min
  **timer retry** (a brownout can be switch bounce, not an empty cell).
- Frame pacing: 30 fps while animating, 5 fps when still; flush sends only
  changed column ranges per page, so a still face costs no I2C traffic.
- Placeholder button/LIS3DH sources exist in Phase 1 only so the all-ON build
  compiles; they produce no events.

## Done
- Phase 0: scaffold. Empty app builds clean.
- Phase 1 (v0.1), verified in software only:
  - builds: default, all-ON, SSD1306, -fanalyzer default + all-ON, all clean;
  - host: `make test` passes, `make analyze` clean, `make preview` renders;
  - default image 222,880 B (flash code 116.8 KB, rodata 43.8 KB),
    DRAM 67.7 KB / 321 KB (21 %).

- 2026-10-23 **Phase 1 confirmed on hardware** (OLED only, USB power): boot log
  `display: SH1106 at 0x3C`, Sasha sees the face. SH1106 confirmed, no shift.
  Not yet checked on hardware: battery power path, light-sleep current.
- Flash size set to 4 MB (boot log detected 4 MB on the Super Mini).

- 2026-10-23 **Phase 3 v0 (case), placeholder dimensions**; restyled as a **robot head** at Sasha's request (antenna = loop, bolt ears, visor, engraved panel + rivets, vents, KC-01 lid): `case/` OpenSCAD
  model (params.scad / keychain_case.scad), shell + lid + caps STLs, PNG
  previews, check.py (clearance >= tol + watertight), all pass; check verified
  to fail on an overlapping layout. Outer 39.6 x 45.3 x 21.6 mm. Buttons:
  3 on the front under the screen (Sasha). OLED 1.3", buttons 6x6 (Sasha).
  OpenSCAD = Homebrew `openscad@snapshot` 2026.10.11 (stable cask disabled).

- 2026-10-23 Case direction: Sasha rejected the box/robot-box and 9 concept
  sketches (case/concepts.scad); wants a goofy robot that prints without
  surprises -> **"Boltik"** (keychain_case.scad): big rounded head, lightning
  antenna = loop, dish + bolt ears, waving/hanging noodle arms, stubby legs,
  bandage + DON'T PRESS engraved, WARRANTY VOID IF ANNOYED + wind-up key on
  the back. 66 x 79 x 21.6 mm. check.py now also checks rounded corners and
  printability (overhangs / bridges from the STLs); verified to fail on bad
  geometry. All checks pass.

- 2026-10-23 Boltik v1 rejected ("too flat, few details, too sharp"): limbs
  were flat because the shell printed face down. **Boltik v2**: body prints
  back down + separate face plate prints face up, so all visible surfaces
  are round/raised; pill-profile limbs, raised bezel/rivets/bandage/blush/
  text, 2x M2 screws via OLED top holes into bosses, spacers. check.py fit
  check is now real 3D (OpenSCAD intersection probes). All checks pass with
  no relaxations; each check shown to fail on broken input.

- 2026-10-23 v2 rejected: limbs stuck to the back (not centred), legs ugly,
  "how will it stand -> make it sit". **Boltik v3**: arms/ears are separate
  teardrop-profile parts on M2 screws at mid-depth (arms posable), legs are
  separate, pinned to the face plate with filament, stretched forward: it
  sits. Keyring = thick ring on top (part of body). Switch moved to the back.
  All checks pass (10 STLs). Found and fixed by checks: switch well vs back
  text, arm reach vs desk, joint bosses printed point-up (overhang).

- 2026-10-23 Sasha found a ready model instead: the **Yeti** from
  Compagnon 309 (MakerWorld 2109424; USB-powered, ESP32-C3 + OLED + touch).
  `case/yeti/`: 3 belly buttons cut into the original body (pockets square
  to the sloped, solid belly; adds no supports); battery power via a new
  electronics carrier + back plate (2 stacked USB-C, switch, LiPo), replacing
  the original tray, whose front wall blocked the buttons. All fit probes
  pass against the real mesh; both new parts print without supports.
  Originals stay local and untracked (third-party design).

- 2026-10-25 **Phase 2 (v0.2), software only**: Sasha has buttons, LIS3DH,
  passive buzzer. Real button driver (level IRQ = light-sleep wake, debounce,
  short/long), LIS3DH (hardware click -> TAP/DOUBLE_TAP on INT1, software
  shake/tilt), buzzer (`FEATURE_SOUND`, LEDC on GPIO10, XTAL clock, chirps per
  reaction, BTN_B_LONG mutes). Main loop waits in `input_wait()` so an IRQ
  ends the idle frame early. BUTTONS/ACCEL/SOUND now default ON. Host
  `input_test` (mutation-checked); default/analyze/ssd1306/all-on/min builds clean.
  Tilt axis/sign are PLACEHOLDERs until mounted.
- 2026-10-25 **Phase 2 on hardware** (USB power, breadboard): OLED, LIS3DH
  (0x19), buttons A/B, buzzer all work. Bench fixes: taps read every frame
  (INT1 alone missed them), tap threshold 24, ringing = clicks <80 ms apart,
  double tap in software (chip's DCLICK never fired), taps dropped around
  button presses (the press jolt read as a tap). Buzzer too quiet with no
  transistor: tunes moved to 1.5-4 kHz, drive cap 3 = loud enough (Sasha).
  Optional bridge mode (− on GPIO20) is in the code, off. ~1 in 7 double taps reads as single.

- 2026-10-25 **Face redesign: "Mochi dots"** (Sasha rejected 3 static styles
  and picked Dasai Mochi from 3 animated directions, then sent a reference
  photo: small round eyes far apart, bold little smile). face.c redrawn;
  new expressions SAD (mood) and LAUGH, DIZZY, KO, EXCITED, WINK (reactions).
  Each expression animates only for a short window (face_is_animating), then
  the loop drops to 5 fps. 2-button controls: A = pet (random cheerful face),
  B = step through every face, A long = sleep, B long = mute, tilt = look.
  Shake = dizzy, shake while dizzy = KO. On the panel: eyes 7 px looked too
  small, now 10 px with every eye shape and the smile scaled; Sasha OK'd it
  ("will improve later").

- 2026-10-25 **v0.3: menu + games** (`main/app/`, pure C, host-tested like
  the face; hardware via app_env_t). B long opens the menu (B next, A choose,
  B back, B long to face; 15 s idle closes it, game screens 30 s unless in
  play). Games: Dice (shake/tap/A), Coin (tap/A, heads = Yeti face), Reaction
  (A press timing; includes ~30-60 ms debounce + frame latency), Catch (tilt
  = position; only offered with an accelerometer), Flappy (A flap, fixed
  33 ms physics ticks). Sound on/off moved into the menu. Best scores + mute
  in NVS (`storage/`, versioned blob). New INPUT_BTN_x_PRESS events (press-
  down) for games; policy_note_activity so A long in a game isn't "sleep".
  5x7 font (`display/font.c`). host/app_test.c caught a wrap bug in
  app_elapsed (future times read as elapsed). Flashed; not yet played by Sasha.

- 2026-10-26 **v0.4: pet life** ("too boring, add more features").
  `pet/`: food, fun, energy (0-100) tick per powered minute (no clock across
  power-off, the pet pauses), XP and levels from games and feeding, age;
  saved every 10 pet-minutes. Needs make the mood sadder or sleepier and show
  a thought bubble (fish / ball / z). Menu is now two lists: FEED (fish,
  berry, cake; refuses when full), GAMES >, PET (stats), SLEEP, SOUND, SETUP,
  BACK; the games list adds Snake (A left, B right, press events, turn queue)
  and Memory (Simon with the two eyes, A/B). LEVEL UP banner. Face: eyes follow
  tilt, upside down = SCARED (needs calibration), picked up after 90 s still =
  hi, freefall = WHEE, idle sneeze and hum; sounds through face cues.
  SETUP calibrates the LIS3DH (lay flat, hold upright -> `input/orient.c`
  board-to-screen map, saved in NVS), so the ACCEL_TILT placeholders now
  describe the screen frame (sign -1). NVS record v2, migrates v1 scores.
  Host tests for all of it; flashed, boot log clean, not yet played.

- 2026-10-26 **v0.5: backstory, LED, songs, simpler controls.** YETI-7 lore
  (`pet/story.c`): a siren alarm (buzzer + red LED + flashing screen) 10 s
  after a need appears and every 20 min while it lasts; random glitches
  (torn picture, garbled chirps, a log line: "SIGNAL LOST"...) every 15-45
  min; a random song every 25-70 min (first ones within minutes of power-on),
  shake to stop it (dizzy + "hmph"), sang to the end = proud. Songs are RTTTL
  strings (`sound/rtttl.c`: Jingle Bells, Korobeiniki, Mountain King, Ode to
  Joy, Twinkle, Fur Elise, Happy Birthday) and the LED flashes per note.
  Jukebox (GAMES > MUSIC). Red LED on GPIO20 with no resistor (weakest pad
  drive + PWM, `led/led.c`). Controls: face A = pet, B = menu; no long
  presses needed (A long = pet, B long on the face = cycle faces, hidden);
  hint line "A:... B:..." on every screen; SLEEP only from the menu.
  Face concepts for the redesign: https://claude.ai/artifact/NKgQjLCCMvRAATcBG9vrg2

- 2026-10-26 **Face: EMO style** (Sasha: "emo style"). No mouth (only yawn
  and sneeze); tall rounded pill eyes 24x30 carry every emotion: happy =
  arches, angry = inner top cut, sad = outer cut + tear, sleepy = lidded,
  scared = small trembling pills + "!". Hearts/stars/spirals/X/chevrons
  enlarged. Mini faces (menu, coin, feeding) use pills too. ~1200 lit px
  awake (was ~750). Flashed, boot clean.
  Sasha: "even bigger" -> eyes 32x42 (from 24x30), all shapes scaled;
  ~2400 lit px awake. Thought bubble and warning sign get a black outline
  where they overlap the right eye.

- 2026-10-26 Face: the > < eyes dropped (Sasha didn't like them): laugh =
  happy arches with quick hops, sneeze "choo" = eyes squeezed to bars.

- 2026-10-26 **v0.6: touch pad + microphone (software; parts not fitted
  yet).** Sasha picked a TTP223 and a mic. TTP223 on GPIO5 (button C was
  never fitted; pull-down, so absent = never touched): pat = happy + boop,
  hold = hearts + purring for as long as it lasts, 3 quick pats = tickle
  (laugh). Mic (MAX4466-type, analog) on GPIO4 (ADC1, DMA at 8 kHz, own
  task, `input/mic_logic.c` host-tested): clap = startled, clap-clap = it
  sings a song (shake to stop), shout = scared, blowing = sneeze, talking =
  it looks up and listens, then babbles back one chirp per syllable. It is
  deaf while the buzzer plays; claps/bangs right after a tap, button or
  shake are dropped (handling noise). Ears only while awake on the face:
  the ADC's DMA keeps the chip out of light sleep. No mic on the pin
  (reads ~0 V) = logged and switched off for good. Flashed with neither
  part fitted: boot clean, `no mic on GPIO4`, no phantom touches.
  Mic thresholds are PLACEHOLDERS until the bench (the log prints every
  sound's length / level / overload / class).
- 2026-10-26 — TTP223, mic and LED soldered; boot sees all three (mic bias
  2233/4095). More reactions (host-tested, flashed, not yet tried by hand):
  - Shaking builds anger (a SHAKE every ~0.8 s of shaking, KO in 3-4 s;
    taps and tilt still wait 1.5 s): each shake within 5 s of the last goes one step
    further: dizzy, grumbling, FURIOUS (slit eyes, teeth, steam, the screen
    flashes), and at the 5th a KO. Then it sulks for 20 s, looking away;
    three pats or one cuddle forgive it.
  - Long petting is one sequence that fits the TTP223, which lets go by
    itself ~7 s into a touch (bench, 2026-10-26): squished under the hand
    (0-1.5 s after the hold), nuzzling with purr ripples and blush (to
    3.2 s), melting with drops falling into a puddle (to 4.8 s), then heart
    overload (hearts beating ever faster, buzzer and LED beat with them).
    Letting go in the overload, which is when the chip gives up anyway,
    plays the finale: a burst of hearts, a sparkle sound, an LED burst.
    Let go sooner: a happy hop, or (melted) it snaps back into shape. A pad
    that holds on gets the finale at 7.5 s, then content hearts.
  - Hangs (story event, first 2-5 min after boot, then every 20-50 min):
    "NOT RESPONDING", a stuck bar, everything else gets an error beep and B
    is blocked. Three shakes (jolts move the bar) reboot it through a
    "YETI-7 BIOS" screen; it reboots itself after 90 s.
  - More animations (host-tested, previewed, flashed; not yet seen on the
    panel): power-on (a line pinches into two bars that spring open into
    eyes, a look around); idle fly (buzzes about, the eyes follow, lands on
    the right eye, blinked off) and idle snow (looks up happily at falling
    flakes, sometimes sneezes after), each ~4% of idle behaviours (~15-20
    per 2 h); tickle = laugh with tears of joy, tickled again within 4 s =
    rolls off the screen and back in; blowing = squinting into gusts, pushed
    aside, then the sneeze; a mouth that moves while it babbles back; waking
    = a big stretch (tall, squat, wobble).
  - LED on everything: blinks for taps, claps, pats; giggle flashes; a slow
    breath while petted (dimmer when dozing); fury flicker, KO fade, a stuck
    flicker while hung, a boot ramp; a flash per chirp when it babbles back.

- 2026-10-27 **v0.7: the rework after it went into the real case.** Sasha
  picked features in the Yeti Feature Picker artifact
  (https://claude.ai/artifact/LntnUsjr31pYgowuuNJPda). Hardware fixes first:
  - SDA and SCL were shorted together after rewiring (found by driving one
    and reading the other); `i2c_bus.c` now logs a held-low or bridged bus
    at boot;
  - the OLED sits rotated 180 degrees: `DISPLAY_UPSIDE_DOWN 1`;
  - buttons left to right on the case are A, B, C = GPIO20, GPIO3, GPIO1
    (A moved to GPIO20, so deep-sleep wake is now B or C).
  Then the firmware:
  - **one rule per button, on every screen**: A do, B next, C back; held C
    = home. The bottom line shows each word above its button. On the face:
    A pets, B menu, C status card (volume, brightness, battery/USB);
  - menu = GAMES / MUSIC / SETTINGS. Games: dice, reaction, catch, flappy,
    snake, and the new **voice** game (shout to jump, louder = higher, hum
    to float; A jumps too, so it plays without a mic). Settings: volume
    (off/quiet/medium/loud, via PWM duty 0/3/12/50 %), brightness
    (dim/normal/bright), calibrate;
  - removed: the tamagotchi (feeding, stats, needs, levels, need alarm),
    coin flip, memory, the sleep menu item, blow/clap/loud reactions (the mic
    now only reports talking; mic.c filters the rest);
  - sleep is now "nobody touched it for 10 min" (no clock, so no fake
    uptime night); picking it up wakes it; lying face down turns the screen
    off (needs Settings > Calibrate);
  - save record v3 (scores, volume, brightness, calibration); a v2 record is
    migrated, keeping scores and calibration;
  - `make preview` also renders the menu and games to `host/out/app.png`.

- 2026-10-28 **v0.8: Maze.** A tilt game in GAMES after Catch: hold it flat,
  tilt to roll the ball to the ring. Mazes are generated (recursive
  backtracker, goal = the deepest cell), cells 16 → 12 → 10 → 8 px level by
  level; a time bar (45 s, + 8 s + 0.12 s per cell for each maze solved, up to
  99 s); one point per maze. The ball rolls with the in-plane tilt minus the
  tilt at the start, so it needn't be perfectly level. New env `tilt_xy`;
  save record v4 (adds `maze_pts`, v3 migrated). Not yet tried on the device.
  Planned (wiring on the wiring page, not soldered): battery divider on
  GPIO2, charger sense from TP4056 IN+ to GPIO21 for a charging animation.

- 2026-10-28 **Flat menus and 7 more songs.** No GAMES / MUSIC / SETTINGS
  level any more (Sasha: fewer clicks, one interface). From the face: B =
  the games list (opens on the last game played), held B = the jukebox
  (opens already playing the last song; B = next song, playing at once), C =
  the status card, with volume / brightness / calibrate after it. C in a
  list goes back to the face. A two-button chord was considered and dropped:
  every single press would have to wait to see if a second one follows.
  Songs: Sasha picked 7 in the Yeti Song Picker artifact
  (https://claude.ai/artifact/L7xH5xbGpzZCryEufvbFWg, db `picks/songs`) from
  huggingface.co/datasets/cosimoiaia/RTTTL-Ringtones: Barbie Girl, Final
  Countdown, Harry Potter, Imperial March, Last Christmas, Mortal Kombat,
  Take On Me, raised 1-2 octaves into the buzzer's 1-4 kHz. Two had the next
  ringtone glued on in the dataset, cut off. The parser now reads "_" as a
  sharp, as that collection does. Then Mountain King, Ode to Joy, Twinkle
  and Für Elise dropped (Sasha): 10 songs, which the Yeti also sings on its
  own (story.c SONG_COUNT 10, a test checks it sings every one).

- 2026-10-28 **Battery percent and the charging animation, on the device.**
  Both dividers soldered with 5.1 kΩ (what Sasha had): battery on GPIO2
  (reads ~5460 mV on USB, shown as "USB"), charger sense IN+ → GPIO21.
  First try read 0 always: the joints between the resistors were also on
  GND; only the last resistor's end may be. Found with a pull-up test (the
  pin read 0 with the internal pull-up on). Animation: cable in (held
  CHARGE_DEBOUNCE_MS) = star eyes, SND_CHARGE, LED rise, wakes it; the HUD
  battery gets a bolt and its fill sweeps up to full (also on USB, switch
  off); full (≥ 100 %, switch on only) = happy + ding, once; cable out =
  surprised, SND_UNPLUG. Status card: bolt and "CHG" (or the %).

- 2026-10-28 **Music animations, a song chooser, the mute icon, the 67 egg.**
  Holding B opens the chooser on the last song, silent (Sasha: the user
  must choose); A plays, B next, C back. Playing: A / C stop (back to the
  list), B next song at once. Nine animations (music_viz.c), picked by Sasha
  in the Yeti Visualizers artifact
  (https://claude.ai/artifact/FmounxUi3W3KfGsELwoJWa, db
  `picks/visualizers`): Dancing Yeti, Bouncing Ball, Synthwave, Vinyl, Yeti
  DJ, Warp, Note Rain, Oscilloscope, Equalizer; one at random per song,
  never the same twice running. The app parses the playing song itself
  (gap 0, so start times add up) and feeds each note to the animation on
  its own clock: no hook from sound.c needed. The song's name shows for
  2.5 s. Sound off: "SOUND OFF" in the chooser. Mute icon (hud_muted) top
  right on the face while volume is 0. Easter egg (game_egg.c): six presses
  of A (each within 0.9 s), then a shake within 3 s: a 7 s "67" show
  (charge-up, 6 and 7 slam in with flashes and shake, shockwaves, the 6-7
  hands with SIX SEVEN scrolling, a spin, an explosion, a fanfare), then an
  excited face. Previews in host/out/app.png.

- 2026-10-28 **Sasha's running order, new game cards, Dino Roar.** Games and
  songs in the order set in the Yeti Running Order artifact
  (https://claude.ai/artifact/FzG9RZ1VHmfEmNqoHkGEmB, db `order/main`):
  Catch, Maze, Flappy, Snake, Dino Roar, Reaction, Dice; songs from Barbie
  Girl to Happy Birthday. The games list opens on Catch the first time.
  Each game's menu screen is a big picture and its name, nothing else
  (art.c). A first try with a framed scene, how-to lines and the best score
  was rejected by Sasha: "keep it simple". The jukebox's list got a
  themed picture per song too (a heart for Barbie Girl, a rocket for Final
  Countdown, Tetris for Korobeiniki, glasses and the scar for Harry Potter,
  a battle station for Imperial March, a cake for Happy Birthday...), with
  the name and a dot per song; the "SONG 3/10" line is gone. Settings keep
  the icon-and-name screen. Voice is now
  Dino Roar (Sasha: "we cannot even name it a voice"): a Chrome-style
  dinosaur that opens its jaw and lets out sound waves while you are loud,
  cacti instead of rocks, "ROAR!" on the title. The item is still
  ITEM_VOICE and the score still `voice_pts`, so saves carry over.
- 2026-10-30 **Easier Flappy, Supergirl.** Flappy: gap 26 → 34 px, pipes
  56 → 70 px apart and slower (1.5 → 1.1 px/tick), softer gravity and flap.
  New 11th song: Supergirl (Reamonn), the chorus of the vocal line (from a Songsterr transcription), with a
  hero-shield picture.

## In progress
- Yeti: waiting for Sasha's review; then caliper measurements (boards,
  battery, switch, OLED position) to replace PLACEHOLDERs. Boltik design
  parked in case/.

## Next
- Phase 2 (stubs): flesh out button + LIS3DH placeholders with TODOs and the
  deep-sleep wake wiring. Only after Phase 1 is confirmed.
- Phase 3 (case): only after hardware confirmation + caliper measurements.

## Open questions
- Battery sense goes on GPIO2 (strapping: confirm it still boots with the
  divider fitted); the mic keeps GPIO4. Charger sense on GPIO21.
- LiPo is now **850 mAh** (Sasha, 2026-10-28; was 300 mAh). Stock Rprog 1.2k
  = ~1 A = ~1.2C: OK if the cell allows 1C, else 1.5k (~800 mA). Supervise
  the first charges. Physical size still needed for the case.
- Battery divider will be 100k + 100k (not 1M + 1M), ratio still 2.
- Super Mini power LED: always-on, drains the cell even in deep sleep. Remove it?
