# Project Nativ: unofficial fork

[This repository](https://github.com/Luufen/nativ-SP) is a community fork of [Project Nativ](https://github.com/TheLostPantheon/nativ),
not an official release; the original README follows [below](#project-nativ). It adds packs for more games, so
Nativ can rebuild your own copy of them, a launcher and in-game menu you can use with a controller or a touch
screen alone, and exports for a Steam Deck and for Android. Its releases are tagged `vX.Y.Z-lfN` (based on upstream
`vX.Y.Z`; `-fpN` before v0.1.5-lf3), are built from the tagged source with `dev/release.sh` on Linux and are unsigned.

**No game comes with this repository or its releases**, as with upstream: you need your own copy of each game, dumped
from a Vita you own, and Sony's firmware update file.
Problems with these builds belong in
[this fork's issues](https://github.com/Luufen/nativ-SP/issues), not upstream's.

## Download and start

Each release has, with `SHA256SUMS` over them:

- **Windows:** `ProjectNativ-<tag>-windows.zip`. Extract the whole zip, then open `ProjectNativ\Project Nativ.exe`.
- **Linux, Steam Deck:** `ProjectNativ-<tag>-linux.AppImage`: mark it executable and run it.
- **Linux without FUSE** (or without AppImages): `ProjectNativ-<tag>-linux.tar.gz`, the same install as one folder.
  `tar xzf ProjectNativ-<tag>-linux.tar.gz`, then run `ProjectNativ/Project Nativ.sh` (Steam: add that file as a
  non-Steam game). Nothing is mounted or unpacked at start.

**Portable:** the zip and the tarball keep everything in their own folder: your games and saves in `library/`, the
settings in `settings/`, caches in `cache/` (a file named `portable` at the top says so; delete it to use your user
folder instead). To update, extract the new release over the same folder: those three folders stay. If you had a
library in your user folder (`%LOCALAPPDATA%\ProjectNativ\Nativ\library`, `~/.local/share/ProjectNativ/Nativ/library`),
the launcher uses it from there and asks once whether to move it into the folder or keep it where it is; your settings
are copied in. The AppImage keeps using your user folder; put a file named `portable` beside the AppImage, or start it
with `--data-dir <folder>`, to keep everything in one folder there too.

The launcher is the only program you start. It also takes a command line (`nativ --help`, on Windows
`"Project Nativ.exe" --help`), for scripts, Steam and other launchers:

| Flag | What it does |
|---|---|
| `--list` | The library, one game per line: title, state (`built`, `added`, `pack`), version, name. |
| `--play <title> [-- <game flags>]` | Starts a game as Play does (built first when needed), without the launcher window. |
| `--build <title>` | Builds a game in the library. |
| `--add <folder>` | Searches a folder and adds what's ready (games, updates, add-ons, licences, firmware). |
| `--data-dir <dir>` | Keeps the library, settings and caches in that folder. |
| `--fullscreen`, `--windowed` | The launcher window, or with `--play` the game, fullscreen or in a window, this time only. |

Exit status 0 means done, 1 that the work failed, 2 a bad command line or a title that isn't in the library. The game's
own flags after `--` are `--fullscreen` and `--windowed`. The variables `ATIV_DUMP`, `ATIV_PREFS` and `ATIV_TASK` still
work.

## Steam Deck

The Deck runs the Linux downloads natively, no Windows layer in between:

- **Desktop Mode:** start `ProjectNativ-<tag>-linux.AppImage` (Properties > Permissions > "Is executable", or
  `chmod +x`), or extract the tarball and run `ProjectNativ/Project Nativ.sh`. The launcher works with the Deck's
  buttons and touch screen alone; add, build and play your games there as on a PC.
- **Game Mode:** on a game's page in the launcher, **Add to Steam** makes a Steam shortcut for that game (it starts
  `nativ --play <title>`, without the launcher window); switch to Game Mode and start it from your library. You can
  also add the AppImage or `Project Nativ.sh` itself as a non-Steam game to get the launcher in Game Mode. Since lf16
  these shortcuts start again: in lf10 to lf15 every run without the launcher window crashed at once.
- **One file per game:** on a PC, a built game's page > **Export** > **Linux / Steam Deck** makes one `.AppImage`
  that holds that game and what it needs. Copy it to the Deck, mark it executable and add it to Steam (Desktop Mode:
  Steam > Add a Game > Add a Non-Steam Game). On Windows the export runs in WSL and needs the one-time Linux toolchain
  setup (`runtime/platform/linux/setup-toolchain.sh`). It holds code made from your copy of the game: keep it for
  yourself.

**Verified on a Steam Deck:** Uncharted: Golden Abyss (EU) at **60 fps in Game Mode**, as an exported AppImage started
from a Steam shortcut: a recorded play-through replayed through Steam ran at 60 fps (median) in scenes with up to about
1400 draws, where lf15 managed 15 or less in Game Mode; the owner then played it at 60 fps ("works perfectly"; a short
run). Medieval Defenders and Soul Sacrifice Delta ran as exported AppImages before. Two changes made the difference:

- The window no longer waits for the screen while it holds the renderer: in Game Mode the compositor (gamescope)
  paces each frame, and the renderer stood still for most of it.
- **Renderer worker:** a played game translates its draws on a thread of their own, beside the game. It is on by
  default when you play (test runs without a window leave it off); `RECOMP_GFX_ASYNC=0` turns it off (Steam: set the
  shortcut's launch options to `RECOMP_GFX_ASYNC=0 %command%`), for a comparison or if a game draws wrongly with it.

**Performance log:** in the game, F10 > Performance > **Record a performance log** (or `RECOMP_PERF_LOG=1`) writes
one line a second to `perflog/perf-<date>-<time>.csv` in the game's folder (for an exported AppImage
`~/.local/share/ProjectNativ/Exported/<title>/perflog/`): frame rate, frame times, draws, CPU and GPU load and where the
renderer spends its time. Attach it to an issue about speed on the Deck. It counts some frames twice while the
renderer worker is on, so read its frame rate as approximate.

## Tested games

A pack is a game's settings file: it tells Nativ how to rebuild that game from your copy, which it recognises by the
SHA-256 of the game's main program. It holds no game code or data. Upstream's four games are unchanged. Frame rate is the highest the game offers in this fork, widescreen the widest picture it fills (16:9: wider screens get black bars at the sides).
Version is the game release the pack supports. Tested: Yes if this fork ran the game, played or in automated test runs. Every game can render at up to 6× the Vita's 960×544; 4K (4×) was
checked in the test runs of all of them except Tearaway, LittleBigPlanet, Resistance: Burning Skies, Gundam Breaker 3,
Phantasy Star Nova, Minecraft, Borderlands 2, Terraria and the two Persona Dancing games from PersonaDancingRecomp.

| Game | Title ID | Version | Tested | Frame rate | Widescreen | Max resolution |
|---|---|---|---|---|---|---|
| Soul Sacrifice Delta (EU) | PCSF00532 | 1.00 | Yes | up to 120 | up to 21:9 | 5760×3264 (6×) |
| Persona 4: Dancing All Night (US) | PCSE00764 | 1.00, 1.01 | Yes | 30 | up to 21:9 | 5760×3264 (6×) |
| Persona 3: Dancing in Moonlight (US) | PCSE01274 | 1.00 | No | 60 | not tested | 5760×3264 (6×) |
| Persona 5: Dancing in Starlight (US) | PCSE01275 | 1.00 | No | 60 | not tested | 5760×3264 (6×) |
| Uncharted: Golden Abyss (EU) | PCSF00001 | 1.00 | Yes | up to 120 | up to 21:9 | 5760×3264 (6×) |
| Tearaway | PCSF00476 | 1.00 | Yes | 30 | not checked yet | 5760×3264 (6×) |
| Medieval Defenders | PCSB00672 | 1.00 | Yes | up to 120 | 16:9 | 5760×3264 (6×) |
| Persona 4 Golden | PCSB00245 | 1.00 | Yes | 30 | up to 21:9 | 5760×3264 (6×) |
| Invizimals: Hidden Challenges | PCSF00415 | 1.00 | Yes | up to 120 | up to 21:9 | 5760×3264 (6×) |
| Invizimals: The Resistance | PCSF00510 | 1.00 | Yes | 30, slower in busy scenes | 16:9 | 5760×3264 (6×) |
| Killzone: Mercenary (EU) | PCSF00243 | 1.00 | Yes | up to 120 | 16:9 | 5760×3264 (6×) |
| WipEout 2048 | PCSD00005 | 1.00 | Yes | up to 120 | up to 21:9 | 5760×3264 (6×) |
| LittleBigPlanet | PCSA00017 | 1.00 | Yes | 30 (60 / 120: the game runs faster) | not checked yet | 5760×3264 (6×) |
| Gravity Rush | PCSF00024 | 1.00 | Yes | 30, slower in busy scenes | 16:9 | 5760×3264 (6×) |
| Ratchet & Clank Trilogy | PCSF00482 | 1.00 | Yes | 30, slower in busy scenes | 16:9 | 5760×3264 (6×) |
| PlayStation All-Stars Battle Royale | PCSF00153 | 1.00 | Yes | 60, slower in busy scenes | 16:9 | 5760×3264 (6×) |
| Injustice: Gods Among Us Ultimate Edition | PCSB00356 | 1.00 | Yes | 60 | 16:9 | 5760×3264 (6×) |
| Street Fighter X Tekken | PCSB00144 | 1.00 | Yes | 60 | 16:9 | 5760×3264 (6×) |
| J-Stars Victory VS+ | PCSE00595 | 1.00 | Yes | 60 | 16:9 | 5760×3264 (6×) |
| God Eater Resurrection | PCSB00874 | 1.00 | Yes | 30 | not checked yet | 5760×3264 (6×) |
| God Eater 2 Rage Burst | PCSB00875 | 1.00 | Yes | 30 | 16:9 | 5760×3264 (6×) |
| Call of Duty: Black Ops Declassified (Russian) | PCSB00232 | 1.00 | Yes | 30, slower in busy scenes | 16:9 | 5760×3264 (6×) |
| LEGO Ninjago: Nindroids | PCSB00500 | 1.00 | Yes | up to 120 | 16:9 | 5760×3264 (6×) |
| XCOM: Enemy Unknown Plus | PCSB00742 | 1.00 | Yes | 30 | 16:9 | 5760×3264 (6×) |
| Batman: Arkham Origins Blackgate | PCSB00353 | 1.00 | Yes | up to 120 | 16:9 | 5760×3264 (6×) |
| Rayman Legends (Chinese) | PCSH00118 | 1.00 | Yes | up to 120 | up to 21:9 | 5760×3264 (6×) |
| Toukiden Kiwami (Chinese) | PCSH00110 | 1.00 | Yes | 60 | 16:9 | 5760×3264 (6×) |
| Sonic & All-Stars Racing Transformed | PCSB00190 | 1.00 | Yes | 30 | 16:9 | 5760×3264 (6×) |
| Freedom Wars | PCSF00446 | 1.00 | Yes | 30 (60 / 120 experimental) | 16:9 (wider experimental) | 5760×3264 (6×) |
| Silent Hill: Book of Memories | PCSB00115 | 1.00 | Yes | not measured yet | not checked yet | 5760×3264 (6×) |
| Resistance: Burning Skies (US) | PCSA00008 | 1.02 | Yes | up to 120 | not checked yet | 5760×3264 (6×) |
| Gundam Breaker 3 (Asia) | PCSH00208 | 1.30 | No (tested by Tzeentchnet) | 30 | not checked yet | not checked yet |
| Phantasy Star Nova (Asia, Chinese) | PCSH00143 | 1.00 | Yes | 30 | not checked yet | not checked yet |
| Phantasy Star Nova (Japan) | PCSG00351 | 1.05 | Yes | up to 60 | not checked yet | not checked yet |
| Minecraft: PlayStation Vita Edition (EU) | PCSB00560 | 1.70 | Yes | about 56 measured in a world; higher not checked yet | not checked yet | not checked yet |
| Borderlands 2 (US) | PCSE00383 | 1.09 | Yes | about 60 measured after Continue on Windows | not checked yet | not checked yet |
| Terraria (US) | PCSE00317 | 1.11 | Yes | 60 on the title screen; smooth in a world on Windows | not checked yet | not checked yet |

Most of these packs were made with the fork's porting tools (below): they hold what Nativ needs to build and boot the
game from your copy, plus frame-rate and widescreen settings found automatically.

Other games may work out of the box but are untested: when you add a game that has no pack, the launcher writes a
first one for it and builds it. Some games then reach their menus or play (AQUA KITTY DX was played this way on
Android); others stop at a missing function or a feature Nativ doesn't have yet.

## What's new compared with Project Nativ v0.1.5

**Games**
- **36 more games** than upstream's 4 (see [Tested games](#tested-games)): they render at **up to 6× the Vita's
  resolution (4K and beyond)**, 10 of them run at **up to 120 fps** and 7 fill **21:9** widescreen.
- **A game without a pack still gets a start:** adding it writes a first pack automatically.

**Launcher**
- **Works with a controller or a touch screen alone**: every page, its own file browser and on-screen keyboard.
- **Library** with a page per game; finds games in other folders and drives, including a **Vita3K** installation.
- **Updates, add-ons (DLC) and cheats** per game; **zRIF licences** accepted wherever a `.rif` is asked for.
- **Data mods (rePatch folders)** per game on its **Mods** page, from a folder or a `.zip`, switched on and off
  without a rebuild; a mod's files are laid over the game's at run time (a mod's program files are refused). The
  Arks-Layer English patch for Phantasy Star Nova (Japan) was played this way.
- **Export** a built game for your own devices: **another Windows PC**, a **Steam Deck** (AppImage) or an **Android**
  phone or tablet (`.apk`). Steam Deck and Android builds run in WSL on Windows and need a one-time toolchain setup
  (`runtime/platform/linux/setup-toolchain.sh`, `runtime/platform/android/setup-toolchain.sh`).
  On ARM64 Android devices with a Qualcomm Adreno GPU, open **Settings → GPU driver** in the exported game app.
  Use **Import ZIP** for an AdrenoTools package such as Mesa Turnip, or choose **System driver** to switch back.
  You can remove installed packages from the same menu. The choice is saved for that app and takes effect when you tap
  **Play**. To get these settings in an existing Android installation, export a new APK.
- **Add to Steam**, also from the Steam Deck's Game Mode (its shortcuts start again since lf16, see
  [Steam Deck](#steam-deck)). English and German; any window size or fullscreen.
- **Command line** (`--list`, `--play`, `--build`, `--add`, `--data-dir`, `--fullscreen`, `--windowed`), **portable**
  zip and tarball (library, settings and caches in the install's folder) and a **Linux tarball** beside the AppImage;
  see [Download and start](#download-and-start).
- Logs and bug reports leave out your user folder and account IDs.

**In the game** (F10, Start + Select, or the PS button)
- **Every Vita and PS TV button on keyboard and controller**, remappable on a drawn controller, with the names printed
  on your pad.
- **On-screen buttons for touch play**; stick sensitivity and dead zone; which controller and camera to use.
- **Game language**: your computer's or any of the Vita's 20.
- **Cheats** (VitaCheat `.psv` files) switched on and off while playing.
- **Performance overlay** (F1): frame times, CPU and GPU load, memory; a **performance log** to a CSV file
  (F10 > Performance).

**Picture and sound**
- **120 fps, widescreen and 4K** in more games.
- **ATRAC9 and AAC audio**, movies with their soundtrack, smoother sound, fixes for games using FMOD.
- **Short waits on time on Windows**: a game's brief sleeps with callbacks (and its vblank waits) took up to 16 ms
  there and cost frames; Phantasy Star Nova (Japan) went from below 30 to 60 fps with this.
- **Steam Deck at 60 fps:** a renderer worker thread and a window that no longer waits for the screen while holding
  the renderer took Uncharted: Golden Abyss from 15 or less to 60 fps in the Deck's Game Mode (see [Steam Deck](#steam-deck)).
- Many GPU fixes: instanced draws, shader subroutines, more texture and render-target formats.

**The Vita's hardware, without a Vita**
- **Webcam as the Vita's camera**, a controller's gyro as its motion sensor.
- **Trophies offline**, with a note when you earn one; text input and system dialogs on screen.
- Games that expect a network see an offline Vita; nothing goes online.

**Compatibility**
- **Many recompiler and system fixes**: more ARM and NEON instructions, game modules loaded while playing, threads,
  fibers, timers, the console's memory budget and its system fonts. These get most of the tested games to their menus.
- **From Tzeentchnet's fork**: **Resistance: Burning Skies** and the fixes it needed (shader byte operations on all
  four colour channels, two-sided stencil, deferred lighting, NGS sound buffers, an option to skip mission intros),
  and **Gundam Breaker 3** (Asia) with more shader, texture and NEON support (its research notes: `docs/games/`).

**For people porting a game**
- **`vitarecomp bootstrap`** builds and tests a new game until no new code turns up; **`hooks-scan`** suggests
  frame-rate and widescreen settings; every test run lists what the game asked for that Nativ doesn't do yet.

## Tested

- Windows 11 (x64): Soul Sacrifice Delta, Persona 4: Dancing All Night, Tearaway and Resistance: Burning Skies (US
  1.02, also at 60 and 120 fps), Killzone: Mercenary (EU, also at 120 fps) and LittleBigPlanet (30 fps, and the hint
  shown for 60 / 120) and Phantasy Star Nova (Asia, past character creation, with an in-game save; Japan 1.05 with the
  English patch as a data mod, at 60 fps) and Minecraft: PlayStation Vita Edition (a world started and played, about
  56 fps) and Terraria (US 1.11, character creation and a world) played from a local build of the same source; for every game, see the Tested column above.
- Steam Deck: Uncharted: Golden Abyss (EU) at 60 fps in Game Mode (from a Steam shortcut), Medieval Defenders and Soul
  Sacrifice Delta, each exported as an AppImage (`vitarecomp export-appimage`).
- Android (Samsung Galaxy Tab S7, Android 13): AQUA KITTY DX and Soul Sacrifice Delta, each exported as an `.apk`,
  played by touch with picture, sound and movies. Uncharted: Golden Abyss reached the prologue on Adreno 650 after
  a fix to HDR framebuffer blending. Killzone: Mercenary reached mission 2, where the frame rate was well below 60.
- Android (Samsung Galaxy S24+, Android 16, Xclipse 940): Uncharted: Golden Abyss now reaches the prologue with the
  Samsung Vulkan system driver. **Severe graphics defects remain on this device.** The test confirms startup and
  prologue entry.
- Each release's Windows launcher passes the controller, touch, scroll, export and screen-fit tests. The Linux
  tarball added, built and started Medieval Defenders in a test on Linux; the launcher AppImage is built and packaged
  only. The current releases are built on
  Linux and have no macOS `.dmg`.

## Contributing with an AI assistant

Most of this fork was written with AI coding assistants (Claude Code, Codex and similar), and new game packs or a
higher frame rate for a game you own are good first contributions made that way. Open the checkout in your assistant,
add your own copy of the game in the launcher first (`+ Add game`, or `vitarecomp add`), and give it one of the prompts
below with `<TITLE ID>` filled in.

Rules for every contribution: a pull request holds only `packs/<title>/` files (`pack.toml`, `hooks.c`,
`shaders.txt`) and tools or fixes, never anything from the game (no code, data, shaders, saves, screenshots, logs with
its bytes) and no paths of your machine. Say what you checked and how: "reached the main menu in a test run" is fine,
"playable" needs you to have played it. One game or one fix per pull request.

**Add a pack for a new game**

```text
Read README.md, docs/developers.md ("Packs, versions and updates") and pipeline/packconf.py. I own <TITLE ID> and added it
to my library. Make a pack for it: run `vitarecomp pack-init <TITLE ID>`, then `vitarecomp bootstrap <TITLE ID>`,
and repeat analyze / recompile / build / test until the game reaches its first menu or stops for a reason you can
name. When it stops, read the test run's GAPS line and the log, fix what the pack can fix (MODULES, seeds,
TEST_FRAMES) and tell me what needs a runtime fix instead, with the evidence. Write down each attempt and its result
before the next one. Don't guess menus or controls: ask me what is on screen and which button to press. At the end
show me the pack's diff, the exact commands and their output, and what was and wasn't checked. Commit nothing from
the game.
```

**Unlock 60 / 120 fps (or widescreen) for a game that has a pack**

```text
Read README.md, docs/runtime.md (Frame rate), pipeline/hookscan.py and runtime/core/hooks.c. For <TITLE ID>, run
`vitarecomp hooks-scan <TITLE ID>` and list the frame-time candidates (1/30 s, frame_dt) and aspect candidates
(960/544). For each candidate, find out from the recompiled code whether it runs once a frame and what it feeds;
then try one at a time as IHOOKS in packs/<TITLE ID>/pack.toml with RECOMP_FRAME_RATES=30,60,120 in RUN_ENV, and
test-run the game at 30, 60 and 120. A site is right only if game speed stays the same (animations, timers,
movement) at every rate; check this with me in a real run before calling it done, because a scene can look fine
while the game runs twice as fast. Write the README table's Frame rate / Widescreen cell from what was actually
seen. Show me the diff, the commands and their output.
```

## Credits

The Persona Dancing packs, Vita3K-style texture packs and their renderer fixes come from
[PersonaDancingRecomp](https://github.com/Ecstatic-Magician/PersonaDancingRecomp) by **Ecstatic-Magician**, merged
with its full history (setup and notes: `packs/PCSE00764/README.md`). [Vita3K](https://github.com/Vita3K/Vita3K) is the
reference for how many of the Vita's system functions behave.
Resistance: Burning Skies' EU pack and the fixes it needed come from [Tzeentchnet's fork](https://github.com/Tzeentchnet/nativ)
by **Tzeentchnet** (taken with their authorship; also offered upstream as their pull request #77), as does the
Gundam Breaker 3 pack with its fixes (this fork's pull request #14, merged with their authorship).

---

> **Below is Project Nativ's original README**, as in [TheLostPantheon/nativ](https://github.com/TheLostPantheon/nativ)
> (v0.1.5), unchanged. The games, features and fixes this fork adds are described above.

<p align="center"><img src="assets/brand/banner.svg" alt="Project Nativ: your PS Vita games, rebuilt natively" width="720"></p>

# Project Nativ

Project Nativ turns PS Vita games into native programs for macOS, Linux and Windows. You bring your own copy of a
game. The tools recompile it on your machine, and it runs like any other program, with extras the Vita never had:
higher resolutions, wider screens, 60 fps and mouse aiming, depending on the game.

## How it works

A Vita game is ARM code written against Sony's system software. Nativ handles the two separately.

- **The game's code is recompiled.** A static recompiler translates the game's ARMv7 code, Thumb-2, VFP and NEON
  included, into C once, ahead of time. That C is compiled into a native program.
- **The system underneath is rebuilt.** Sony's own libraries that a game loads, such as libgxm, libc and FIOS, are
  recompiled with it from your firmware. Below them, a runtime stands in for the Vita's kernel and hardware: it runs
  the game's threads, draws its graphics on your GPU and plays its sound.

[How it works](docs/architecture.md) goes into the design.

## Nothing of the game ships

This repository holds no game code, game data or firmware, and nothing we distribute ever will. You supply your own
copy of each game and Sony's firmware update file, and the tools decrypt and recompile them on your machine.

## Supported games

| Game | Release | Title ID | Versions |
|---|---|---|---|
| Killzone: Mercenary | US | PCSA00107 | 1.00, 1.12 (recommended: Botzone and online play) |
| Uncharted: Golden Abyss | US | PCSA00029 | 1.00 |
| Unit 13 | US | PCSA00014 | 1.00 |
| Shovel Knight | US | PCSE00640 | 1.00 |

Each game's pack supports exact releases, recognised by the SHA-256 of the game's main program. Game-specific fixes,
such as Killzone's physics fix or the widescreen cameras, patch code at fixed addresses, so they only fit the build
they were written for. Your game data doesn't need to match; the main program does.

Not supported yet: other regions (EU, JP, Asia) and the later updates of Uncharted (1.03), Unit 13 (1.01) and Shovel
Knight (1.08, Treasure Trove). The launcher recognises them and tells you which release it supports. It keeps an
unsupported update, and the game runs without it. Porting a new release starts with `dev/hookmap.py`, which carried
43 of Killzone 1.12's 46 hooks over from 1.00 by matching code.

## Getting started

**Players** use the launcher. It walks you through first-run setup, then builds and starts your games. Build it with
`launcher/build.sh`; the result is `launcher/nativ`.

**Developers** can run each stage of the pipeline with `./vitarecomp` from the top of the checkout. Run it without
arguments for the full list.

On Windows, use the Python driver directly from PowerShell instead of the zsh wrapper:

```powershell
.\.venv\Scripts\python.exe .\pipeline\vitarecomp.py --help
```

Replace `--help` with a stage and its arguments. The Windows player release includes its own Python and toolchain;
the [developer guide](docs/developers.md#making-a-release) describes building that distribution on macOS or Linux.

The main stages, in order:

| Command | What it does |
|---|---|
| `firmware <PUP>` | Decrypts Sony's firmware update file. |
| `scan <folder>` | Finds games, updates and firmware files in a folder, at any depth. |
| `add <copy>` | Adds a game or its update. `import` takes one that's already decrypted. |
| `analyze <title>` | Finds the game's functions. |
| `recompile <title>` | Translates them to C. |
| `build <title>` | Compiles the game. |
| `run <title>` | Plays it. |
| `check <titles>` | Runs the regression gate. |

The pipeline's Python needs the packages in `requirements.txt`, installed in `.venv` at the top of the checkout.
The Windows player release includes its Python in `python/`.

### Your library

Your games, firmware, licences, builds and saves live in one library folder:

- `dump/` inside a checkout, which git ignores;
- otherwise your per-user data folder: `~/Library/Application Support/ProjectNativ/Nativ/library` on macOS, under
  `~/.local/share` on Linux and under `%LOCALAPPDATA%` on Windows;
- `library/` in a portable install (a file named `portable` at its top) or in the launcher's `--data-dir <folder>`;
- or wherever `ATIV_DUMP` points.

### Installing

`./vitarecomp dist <folder>` writes an install that holds only what ships. Running it again over an install it wrote
updates the program and leaves your library alone. Two scripts add what a player's machine lacks:

- **Linux:** `dev/linux-install.sh <folder>` adds its own compiler and Python, for machines like the Steam Deck that
  have neither.
- **macOS:** `dev/macos-install.sh <folder>` adds SDL, SPIRV-Cross, FFmpeg's H.264 decoder and Python, and a
  `Project Nativ.app` to start it. The player needs only Apple's Command Line Tools, which the app offers to install.
  Build the libraries once with `runtime/platform/macos/setup-toolchain.sh`.

The [player guide](docs/player-guide.md) covers the rest from the player's side: first run, controls, online play and
reporting a bug.

## Repository layout

| Folder | What's in it |
|---|---|
| `launcher/` | The launcher (`hub.cpp`): your library, first-run setup and settings. It runs the pipeline for you. |
| `pipeline/` | The `vitarecomp` driver, plus game and licence handling. |
| `recompiler/` | The static recompiler, its runtime header `rt.h`, and seeds for functions the analysis misses. |
| `runtime/` | What a recompiled game runs on: `core/` (kernel and libraries), `gfx/` (graphics), `audio/`, `media/`, `input/`, `net/` (networking and online play), `ui/` (window and in-game menu), `platform/`, `debug/` and `tests/`. `build.py` builds it. |
| `packs/<title>/` | One folder per game: `pack.toml` (versions, modules, hooks and settings), `hooks.c` (that game's code patches) and `shaders.txt` (its shader recipes). |
| `decrypt/` | Decrypting firmware updates and game packages, converting SELF to ELF, and the table of Sony's function names. |
| `servers/` | Killzone's login server stand-in (`kzserver/`) and the relay node for online play (`nativ-node/`). |
| `dev/` | Developer tools: replays, diffs and bisects, the remote gate, tests and firmware research. |
| `third_party/` | Dear ImGui, stb, LibAtrac9, Monocypher and the HE-VAG tables, each with its licence. |
| `docs/` | The documentation listed below. |
| `assets/brand/` | The logo, wordmark and app icons, all drawn from one geometry by `make.py`. |

## Documentation

- [Player guide](docs/player-guide.md): installing, first run, controls, online play, known issues and bug reports.
- [How it works](docs/architecture.md): the design, layer by layer.
- [Working on Nativ](docs/developers.md): the pipeline, packs and versions, and working across machines.
- [The runtime in detail](docs/runtime.md): display timing, audio, the GPU backend, controls, hooks and metrics.
- [Verification](docs/verification.md): the regression gate and how the translation and renderer are checked.
- [Debugging a game](docs/debugging.md): logs, captures, replays and the renderer's switches.
- [Online play](docs/online.md): matchmaking, finding other players, and the encrypted links between them.
- [The shader compiler](docs/shader-compiler.md) and [the USSE instruction set](docs/usse-isa.md): the Vita's GPU
  shaders and how we translate them.

## Licence

Project Nativ is free software under the GNU General Public License, version 2 or (at your option) any later version:
see [LICENSE](LICENSE). Third-party code keeps its own licence, and [THIRD_PARTY.md](THIRD_PARTY.md) lists it with
the projects parts of Nativ are ported from.

Nativ contains no Sony or game code. You supply your own games and the firmware from Sony's site; the games are
recompiled on your machine and never leave it.

## AI disclaimer

Nativ is made with the help of AI tools: much of its code and documentation was written with AI assistance, under its
developer's direction, and tested on the games themselves.

## Supporting Nativ

Nativ is free, and everything in it is for everyone. If you'd like to support its development, you can leave a tip on
[Ko-fi](https://ko-fi.com/kingtorro).
