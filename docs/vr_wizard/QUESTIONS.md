# Daytona Wizard unknowns (batch at playtest gate, not repeated CSV requests)
Q1. Exact original EMULATOR.EXE build/hash and immutable backup? Why: RVA 0x000BABAD may not survive unrelated builds. Default: keep existing exact-source gate, do not auto-detect from unknown binaries.
Q2. In actual Quest 3 VDXR run, does live race road/vehicle geometry appear differently in both eyes? Why: HMD Stage 2 gate. Default: UNTESTED.
Q3. What game-world axes/metres and matrices drive original Daytona shader POSITION? Why: accurate Stage 1 tracked 6DoF. Default: no unverified matrix write.
Q4. Which draw sites are world, HUD, menu, particles? Why: VR-safe Stage 8. Default: unsupported elements remain flat without artificial depth.
Q5. How should VR menu/controller bindings interact with MOZA R3? Default: preserve existing wheel, keyboard and separate FFB.
