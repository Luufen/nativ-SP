# Release follow-up

- Android on Samsung Xclipse 940. Investigate the severe graphics defects in the Uncharted: Golden Abyss prologue.
  The Vulkan startup crash is fixed.
- Android on Adreno 650. Profile Killzone: Mercenary in mission 2, where it falls short of 60 fps.
- Borderlands 2. Check widescreen and higher render resolutions for the US 1.09 pack.
- ISSUE-19, 2026-11-24. Play applies a compatible installed update to a stale import and requires 1.70 for PCSB00560. Exports use the imported data tree. Six regression tests and all 22 synthetic APK checks passed. Reporter verification on Vega 8 and Android remains open.

- MALI-01, 2026-11-24. Android Vulkan startup diagnostics and readback wait breakdown are implemented. The no-GPU path now handles a failed swapchain claim. The WSL runtime suite passed (39 entries, one trace-only check skipped), the synthetic Android builds passed 15 checks, and an llvmpipe fixture verified device rejection, swapchain claim rejection, and first frame submission. Stock Mali startup and performance are pending a device trace.
