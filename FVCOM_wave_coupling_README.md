# Memo: Wave–Coupling Hack Branch (WSL / PETSc / SWAN)

## 1. Context

- **Model:** FVCOM 5.0 + SWAN + PETSc 3.18.6  
- **Platform:** Ubuntu / WSL  
- **Case:** `inlet_check` wave–current coupling  
- **Goal:** *“Run without segfaults / bounds errors”*  
  - ⚠️ This is **not** yet a scientifically vetted configuration — it’s a debug / hack branch to get the coupled run stable.

---

## 2. Summary of Major Changes

### 2.1 `make.inc`

**Libraries / paths**

- PETSc / NetCDF / METIS:
  ```make
  PETSC_DIR  = /home/jinikeda/petsc
  PETSC_ARCH = arch-linux-c-opt
  INCDIR = -I/usr/include
  LIBDIR = -L/usr/lib/x86_64-linux-gnu
  ```
- PETSc integration:
  ```make
  include ${PETSC_DIR}/lib/petsc/conf/variables
  LIBS = $(LIBDIR) ... $(PETSC_LIB)
  ```

**Preprocessor / feature flags**

Enabled:

- `-DDOUBLE_PRECISION -DSINGLE_OUTPUT`
- `-DWET_DRY`
- `-DMULTIPROCESSOR`
- `-DLIMITED_NO`
- `-DGCN`
- `-DWAVE_CURRENT_INTERACTION`
- `-DEXPLICIT`
- `-DUSE_NETCDF4`

ICE disabled at compile time:

- ```make
  FLAG_5 =
  ```
  (i.e., no `-DICE`)

**Debug / optimization flags**

- ```make
  DEBFLGS = -g -fbacktrace -fcheck=all
  OPT     = -O0
  ```
- Removed `-ffpe-trap=invalid,zero,overflow` so the run doesn’t die on every FP quirk while still using `-fcheck=all`.

**Archiver**

- ```make
  AR     = ar rc
  RANLIB = ranlib
  ```

**Effect:**  
Code builds cleanly with PETSc and NetCDF; FPE traps are relaxed; ICE code paths are effectively disabled even if some source still mentions ICE.

---

### 2.2 `mod_report.F`

**Previous problem**

With `-DICE` and `ICE_MODEL=F`, ice arrays such as `UICE2`, `VICE2`, `AICE1` could be size 0 or not allocated:

- `Index '1' of dimension 1 of array 'uice2' outside of expected range (0:0)`
- `Allocatable argument 'uice2' is not allocated`

**Change**

Wrap ICE diagnostics (SUM / MAXVAL / MINVAL of AICE / VICE / UICE) with allocation and size checks, conceptually:

```fortran
# if defined (ICE)
  IF (ALLOCATED(UICE2) .AND. N > 0) THEN
     SBUF(18) = SUM(DBLE(AICE1(1:N)))
     SBUF(19) = SUM(DBLE(VICE1(1:N)))
     SBUF(20) = SUM(DBLE(UICE2(1:N)))
     SBUF(21) = SUM(DBLE(VICE2(1:N)))
  ELSE
     SBUF(18:21) = 0.0D0
  END IF
# endif
```

Similar guarded blocks were used for `MAXVAL` and `MINVAL`.

**Note:**  
With ICE now turned off in `make.inc`, this code is currently dormant but kept for a future ICE-capable branch.

---

### 2.3 `swancom1.F` (SWAN – Wave Action Solver)

**Original runtime errors**

- `Index '22' of dimension 2 of array 'DAM' above upper bound of 21`
- Then: `Index '22' of dimension 2 of array 'AC2' above upper bound of 21`

**Cause**

`WWINT(6)` (stored in `ISP1`) sometimes returned **22** while `MSC = 21`. The code did:

```fortran
ISMAX = 1
ISP1  = WWINT(6)

DO IS = 1, MSC
  IF (SPCSIG(IS) < (PTRIAD(2) * SMEBRK)) ISMAX = IS
END DO

ISMAX = MAX(ISMAX, ISP1)   ! can become 22

AMAX = MAXVAL(AC2(:,:,IG))

DO ISC = 1, ISMAX          ! ISC = 1..22 if ISP1=22
  DO IDDUM = 1, MDC
    IDC   = MOD(IDDUM - 1 + MDC, MDC) + 1
    DAMAX = MIN(DAM(IDC,ISC), MAX(XR*AC2(IDC,ISC,IG), AFILT))
    ...
  END DO
END DO
```

So when `ISP1 = 22`, both `DAM(IDC,22)` and `AC2(IDC,22,IG)` were accessed out-of-bounds.

**Hacks applied**

1. **Enlarge `DAM` by one spectral column**

   ```fortran
   ! OLD:
   ! REAL :: DAM(MDC,MSC)

   ! NEW (debug hack 2025-12-04):
   REAL :: DAM(MDC,MSC+1)
   ```

2. **Initialize `DAM` explicitly**

   ```fortran
   DAM(:,:) = 0.0
   ```

3. **Clamp `ISMAX` to valid range**

   ```fortran
   ISMAX = MIN(MSC, MAX(ISMAX, ISP1))
   ```

**Effect**

- Prevents out-of-bounds on both `DAM(:,ISMAX)` and `AC2(:,ISMAX,IG)` even when `WWINT(6) > MSC`.
- This is a **numerical safety hack**, not a change based on SWAN physics or documentation.

---


Physical correctness caveat (in case Future You wonders)

These are numerical safety hacks, not a scientifically derived change to SWAN.
If WWINT(6) really wants to use bin 22 and MSC=21, that likely signals a mismatch between:
how SWAN’s spectral grid is configured,
and how many bins FVCOM/SWAN arrays were allocated for.
By clamping ISMAX to MSC, you’re effectively ignoring any “extra” frequency bin that SWAN thinks is there.
But for this branch, your goal was: “make the inlet_check wave-coupled run not crash” — and this does that.

### 2.4 `ice_coupling.F`

**Issue (when ICE was enabled)**

With `-DICE`, compilation failed:

- `use all_vars, only: T_air, QA_AIR, DSW_AIR, QPREC, CLOUD`
- These symbols do **not** exist in your FVCOM `all_vars` module → “symbol not found” and “no implicit type” errors.

**Resolution in this branch**

Instead of fixing the coupler + `all_vars`, ICE was disabled globally:

- `FLAG_5 =` (no `-DICE`)

So `ice_coupling.F` is no longer in the active preprocessor path and isn’t part of the build for this run.

**Note:**  
`ice_coupling.F` remains modified and should be reviewed if ICE is re-enabled in a future branch.

---

### 2.5 Other Modified FVCOM Core Files

Modified files include:

- `adv_uv_edge_gcn.F`
- `adv_uv_edge_gcy.F`
- `external_step.F`
- `internal_step.F`
- `mod_petsc.F`
- `mod_probe.F`
- `mod_startup.F`
- `mod_station_timeseries.F`

These likely contain some mix of:

- extra `print*` / logging,
- small defensive edits (bounds checks, guards),
- PETSc / nesting / wave–coupling wiring.

**Memo to future self:**

> Treat these as **dirty** until reviewed. Before merging to `main` or trusting results:
> - revert pure-debug changes,
> - and **document + justify** any numerical or algorithmic edits.

---

### 2.6 New File: `ice_init_0.F`

- Currently **untracked**.
- Likely a scratch / experimental ice initialization routine.
- Not used in this wave-only configuration, since ICE is disabled.

**Decision pending:**

- Either:
  - add and wire it properly into ICE build paths in a dedicated ICE branch, or  
  - delete / ignore if it was just experimental and won’t be used.

---

## 3. ICE-Related Issues and How They Were Avoided

### 3.1 `mod_report` ICE Arrays

Errors seen:

- `Index '1' ... outside of expected range (0:0)`
- `Allocatable argument 'uice2' is not allocated`

**Root cause**

- Compiled with `-DICE`
- Running a case with `ICE_MODEL=F` and no ice fields → ICE arrays not allocated or zero-sized.

**Patch (conceptual)**

```fortran
# if defined (ICE)
  IF (ALLOCATED(UICE2) .AND. N > 0) THEN
     SBUF(18) = SUM(DBLE(AICE1(1:N)))
     SBUF(19) = SUM(DBLE(VICE1(1:N)))
     SBUF(20) = SUM(DBLE(UICE2(1:N)))
     SBUF(21) = SUM(DBLE(VICE2(1:N)))
  ELSE
     SBUF(18) = 0.0D0
     SBUF(19) = 0.0D0
     SBUF(20) = 0.0D0
     SBUF(21) = 0.0D0
  END IF
# endif
```

Same logic applied to MAX/MIN sections.

So: if ice arrays don’t exist or `N=0`, diagnostics fall back to zero instead of crashing.

### 3.2 `ice_coupling.F` Build Errors

As described above:

- ICE coupler expects atmospheric fields (`T_air`, `QA_AIR`, `DSW_AIR`, `QPREC`, `CLOUD`) in `all_vars` which this FVCOM configuration doesn’t provide.
- Rather than patch the coupler, ICE was turned off at compile time (`FLAG_5 =`), so:
  - ICE physics are disabled at build **and** in the namelist (`ICE_MODEL=F`),
  - and the `uice2` / `aice` / `ice_coupling` issues are avoided for this branch.

---

## 4. SWAN / Wave Coupling Hacks (DAM & AC2)

This is the main wave-side hack.

**Symptoms**

- Crashes in `swompu2_` (`swancom1.f90`) with:
  - `Index '22' of dimension 2 of array 'dam' above upper bound of 21`
  - then similar for `AC2`.

**Key pattern**

- Spectral dims: `(MDC, MSC)` with `MSC = 21`.
- `WWINT(6)` → `ISP1` could be 22.
- `ISMAX = MAX(ISMAX, ISP1)` meant loops used `ISC = 1..22`, but arrays were dimensioned only to 21.

**Fixes**

1. `DAM` enlarged to `(MDC, MSC+1)` and zeroed.
2. `ISMAX` clamped:
   ```fortran
   ISMAX = MIN(MSC, MAX(ISMAX, ISP1))
   ```

**Result**

- Avoids out-of-bounds on `DAM` and `AC2`.
- **Caveat:** if SWAN conceptually needs that “bin 22”, this is sweeping a config mismatch under the rug. For this debug branch, the trade-off was accepted to get a non-crashing run.

---

## 5. Future TODO List (Cleanup Branch)

### A. Wave–SWAN Consistency

1. **Understand `WWINT(6)` vs `MSC`**
   - Trace where `WWINT` is set and what it represents (triad index, frequency limit, etc.).
   - Decide whether the spectral grid should legitimately have `MSC+1` bins, or if this is an off-by-one error.

2. **Replace DAM/AC2 hacks with a principled fix**
   - Ideal solutions:
     - Configure SWAN so `ISP1 ≤ MSC` always, **or**
     - Explicitly define DAM/AC2 dimensions to match the conceptual grid (e.g., `1:MSC+1`) and document it.
   - Keep `ISMAX = MIN(MSC, ...)` as a last-resort safety check.

3. **Regression test wave coupling**
   - Run a known SWAN test case or FVCOM wave–coupled benchmark.
   - Compare:
     - Hsig, mean period, directional spectra vs reference.
   - If differences appear, track whether they come from the DAM/AC2 changes.

---

### B. ICE Model Re-Enablement (Separate Branch)

1. **Decide policy**
   - Either fully support ICE for this configuration, **or**
   - remove ICE completely for this application.

2. **If keeping ICE**

   - Re-enable:
     ```make
     FLAG_5 = -DICE
     ```
   - Fix `ice_coupling.F`:
     - Ensure `all_vars` defines `T_air`, `QA_AIR`, `DSW_AIR`, `QPREC`, `CLOUD`, or
     - guard these fields with compile-time / runtime checks.
   - Confirm ICE arrays allocation logic:
     - Keep `ALLOCATED(...)` and `N>0` guards in `mod_report.F`.
     - Ensure arrays are allocated when `ICE_MODEL=T` in namelist.

3. **If dropping ICE completely**

   - Strip ICE-related sections from:
     - `mod_report.F`
     - `mod_action_ex` ICE codepath
     - `ice_coupling.F` and other `ice_*` modules
   - Remove ICE-related namelist options that are never used.

---

### C. PETSc & Build System Cleanup

1. **PETSc usage**

   - In `mod_petsc.F`, confirm only necessary PETSc features are used.
   - Double-check compatibility with PETSc 3.18.6 (no deprecated calls).

2. **Debug vs production flags**

   - For production / “science” runs:
     ```make
     OPT     = -O2
     DEBFLGS = -g -fbacktrace
     ```
     (no `-fcheck=all`).
   - Keep a separate debug profile / target with `-fcheck=all`.

3. **`makedepends`**

   - Re-generate `makedepends` after the codebase stabilizes.

---

### D. Clean Up Core FVCOM Modifications

For each of:

- `adv_uv_edge_gcn.F`
- `adv_uv_edge_gcy.F`
- `external_step.F`
- `internal_step.F`
- `mod_petsc.F`
- `mod_probe.F`
- `mod_startup.F`
- `mod_station_timeseries.F`
- `mod_report.F`
- `swancom1.F`
- `ice_coupling.F`
- `ice_init_0.F` (if kept)

Do in the cleanup branch:

1. `git diff` against upstream / a clean baseline.
2. Classify each change:
   - **(A) Debug / logging only** → probably revert.
   - **(B) Defensive numerics (bounds checks, guards)** → keep, but:
     - document them in comments,
     - and ensure they don’t silently alter physics.
   - **(C) Physical / algorithmic changes** → document both in code and in a CHANGELOG / NOTES section.

3. Add small dated comments in code for persistent changes, e.g.:

   ```fortran
   ! 2025-12-04 (JI): clamp ISMAX to avoid OOB when WWINT(6) > MSC.
   ```

---

## 6. Suggested Commit Message Skeleton

You could summarize this branch as:

**Subject:**

> FVCOM wave–coupling hacks for WSL + PETSc + SWAN (inlet_check)

**Body (short):**

- Configure `make.inc` for Ubuntu/WSL, PETSc 3.18.6, and system NetCDF.
- Enable wave–current interaction and NetCDF-4; disable ICE at compile time.
- Add guarded ICE diagnostics in `mod_report.F` (inactive with ICE off).
- Add defensive fixes in `swancom1.F`:
  - extend `DAM` to `(MDC,MSC+1)`,
  - zero-initialize `DAM`,
  - clamp `ISMAX` to spectral dimension: `ISMAX = MIN(MSC, MAX(ISMAX, ISP1))`
    to prevent out-of-bounds on `DAM` and `AC2`.
- Relax FPE traps while keeping `-fcheck=all` for debugging.
- Mark several core files as modified for wave–PETSc debugging; to be cleaned in a future branch.
