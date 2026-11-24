# Release follow-up

- Android on Samsung Xclipse 940. Investigate the severe graphics defects in the Uncharted: Golden Abyss prologue.
  The Vulkan startup crash is fixed.
- Android on Adreno 650. Profile Killzone: Mercenary in mission 2, where it falls short of 60 fps.
- Borderlands 2. Check widescreen and higher render resolutions for the US 1.09 pack.
- ISSUE-19, 2026-11-24. Play applies a compatible installed update to a stale import and requires 1.70 for PCSB00560. Exports use the imported data tree. Six regression tests and all 22 synthetic APK checks passed. Reporter verification on Vega 8 and Android remains open.

- MALI-01, 2026-11-24. Android Vulkan startup diagnostics and readback wait breakdown are implemented. The no-GPU path now handles a failed swapchain claim. The WSL runtime suite passed (39 entries, one trace-only check skipped), the synthetic Android builds passed 15 checks, and an llvmpipe fixture verified device rejection, swapchain claim rejection, and first frame submission. Stock Mali startup and performance are pending a device trace.

- ANDROID-REPORT-01, 2026-11-24. Android Settings exports session logs or a crash report as a ZIP through the system Save As picker. The Android 13 emulator saved both exports to the selected folder, preserved both session logs with redaction, included an Android-recorded synthetic native crash, and produced no file on picker cancellation. All 22 synthetic APK checks passed.

- ANDROID-TOUCH-MENU-01, 2026-11-24. A round button between Select and Start on the Android touch overlay opens the existing F10 menu. Android 13 emulator verification covered tap, cancel, drag-off, F10, a saved 60 FPS setting, and Resume. Touch layout checks passed 1984/1984, the save-dialog fixture passed 18/18, the WSL runtime suite passed, and both Android ABIs linked.

- TERRARIA-01, 2026-11-24. Share compiled bodies for alternate entries with the same decode. The synthetic shared-entry check and all 22 recompiler fixtures passed. On the same Terraria analysis, generated C fell from 17.6 GB to 144 MB and the Windows host linked.

- TERRARIA-01, 2026-11-24. The ARM MLAS emitter sets N and Z from the wrapped result. All 23 recompiler fixtures passed. Terraria passed the previous instruction stop on native Windows.
