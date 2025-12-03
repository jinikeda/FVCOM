# FVCOM build notes (no ice / no wave, WSL Ubuntu)

These are the **minimum steps** I used to get FVCOM compiled and running
without the ice and wave modules on this machine.

Environment is roughly:

- Ubuntu (WSL)
- `mpif90` / `mpicc` (MPICH)
- NetCDF + NetCDF-Fortran
- METIS
- FVCOM tree under `~/Desktop/FVCOM`

---

## 0. Directory layout I assume

```text
~/Desktop/FVCOM/
  ├── src/         # FVCOM source + makefile + make.inc
  ├── run/         # executable + tst_run.nml + INPDIR + OUTDIR
  └── libs/
      └── install/ # contains libjulian, headers, etc.
```

---

## 1. `make.inc` – compiler + flags

In `~/Desktop/FVCOM/src/make.inc`:

### 1.1 Compilers

```make
CPP      = /usr/bin/cpp
COMPFLAG = -DGFORTRAN

FC       = mpif90
CC       = mpicc
CXX      = mpicxx
```

### 1.2 Debug-friendly Fortran flags

```make
DEBFLGS = -g -fbacktrace
OPT     = -O0          # keep this low for debugging
CFLAGS  = -O3

FFLAGS  = -g -fbacktrace -O0 $(INCS)
```

### 1.3 Include & library paths

```make
INCDIR = -I/usr/include
LIBDIR = -L/usr/lib/x86_64-linux-gnu

INCS = $(INCDIR) $(IOINCS) $(GOTMINCS) $(BIOINCS)        $(VISITINCPATH) $(PROJINCS) $(DTINCS) $(PARTINCS)        $(PETSC_FC_INCLUDES)

LIBS = $(LIBDIR) $(IOLIBS) $(DTLIBS)
```

### 1.4 Feature flags (no ice, no waves, WET_DRY on)

Make sure the CPP flags include something like:

```make
FLAG_1  = -DDOUBLE_PRECISION
FLAG_2  = -DSINGLE_OUTPUT
FLAG_3  = -DWET_DRY          # must match WETTING_DRYING_ON in namelist
FLAG_4  = -DSPHERICAL
FLAG_8  = -DMULTIPROCESSOR
FLAG_10 = -DLIMITED_NO
FLAG_15 = -DGCN -DMPDATA -DTVD
FLAG_44 = -DUSE_NETCDF4      # using NetCDF-4 I/O
```

and then:

```make
CPPARGS = $(CPPFLAGS) $(COMPFLAG)   $(FLAG_1) $(FLAG_2) $(FLAG_3) $(FLAG_4)   $(FLAG_8) $(FLAG_10) $(FLAG_15) $(FLAG_44)
```

NetCDF + METIS + julian:

```make
IOINCS = -I/usr/include
IOLIBS = -lnetcdff -lnetcdf -lmetis

DTINCS =
DTLIBS = -L$(TOPDIR)/libs/install/lib -ljulian
```

---

## 2. `makefile` – ensure right objects & stubs are built

In `~/Desktop/FVCOM/src/makefile`:

### 2.1 Add `swmod1.F` and wave stubs to `MODS`

Make sure the SWCOMM3 module and stubs are in `MODS`:

```make
MODS  = swmod1.F       \
        mod_prec.F     sinter.F        mod_types.F     mod_time.F      \
        mod_main.F     mod_spherical.F mod_utils.F     ocpcomm4.F      \
        mod_clock.F    eqs_of_state.F  mod_interp.F    mod_par.F       \
        mod_par_special.F              mod_ncll.F      mod_nctools.F   \
        mod_wd.F       mod_sng.F       mod_heatflux.F  mod_solar.F     \
        mod_bulk.F     mod_input.F     mod_force.F     mod_obcs.F      \
        mod_petsc.F    \
        mod_semi_implicit.F            mod_non_hydro.F mod_set_time.F  \
        mod_startup.F  mod_wqm.F       mod_ncdio.F     mod_setup.F     \
        mod_newinp.F   particle.F      linklist.F      mod_lag.F       \
        mod_northpole.F mod_pwp.F      mod_dye.F       \
        mod_optimal_interpolation.F    \
        mod_report.F   mod_probe.F     mod_gotm.F      mod_balance_2d.F\
        mod_tridiag.F  mod_scal.F      mod_meanflow.F  mod_obcs2.F     \
        mod_obcs3.F    mod_sed.F       mod_enkf.F      mod_etkf.F      \
        mod_rrk.F      mod_rrkf_obs.F  mod_rrkassim.F  mod_enkf_ncd.F  \
        enkf_ncdio.F   mod_enkf_obs.F  mod_enkfassim.F mod_assim.F     \
        mod_nesting.F  mod_visit.F     mod_plbc.F      mod_dam.F       \
        mod_station_timeseries.F       mod_sparse_timeseries.F         \
        mod_boundschk.F mod_esmf_nesting.F      \
        mod_cstms_vars.F   mod_flocmod.F   mod_sed_cstms.F             \
        mod_fluid_mud.F    mod_tvd.F       mod_mld_rho.F mod_vegetation.F \
        mod_heatflux_sediment.F mod_vector_projection.F \
        stubs_icing_upcase.F stubs_wavecomm.F
```

Important bits:

- `swmod1.F` – defines `module SWCOMM3` (used by `mod_northpole`, `mod_esmf_nesting`).
- `stubs_icing_upcase.F` – dummy icing routines only (no SWCOMM3 here).
- `stubs_wavecomm.F` – dummy `SWAPAR1` / `SPROXY` for when WAVE_ON=F.

### 2.2 Keep explicit dependencies

Leave or add the helpful dependencies:

```make
# Explicit module dependencies to ensure correct build order
mod_setup.o:     mod_setup.F
mod_nesting.o:   mod_setup.o
particle.o:      mod_par.o
mod_nctools.o:   mod_par.o
mod_utils.o:     ocpcomm4.o
```

The rest of the object list (`MAIN`, `BIOGEN`, etc.) stays as in the original.

---

## 3. Stub sources for ice & waves

Only the **stubs** needed to be touched/added, not core physics.

### 3.1 `stubs_icing_upcase.F`

Located in `src/`. Ensure:

- It uses `!` comments (not `C` in column 1, since it’s going through `.F` → `.f90`).
- It **does not** define `module SWCOMM3` or any `MDC/MSC` symbols.

It should just provide the dummy routines expected by `mod_ncdio` (e.g. `icing`, `upcase`, etc.), with empty bodies.

### 3.2 `stubs_wavecomm.F`

Create `src/stubs_wavecomm.F` with fixed-form Fortran:

```fortran
      SUBROUTINE SWAPAR1(I1, ISS, ID, DEP, KWAVELOC, CGLOC)
!     Dummy stub: wave coupling not active in this build.
      INTEGER I1, ISS, ID
      REAL    DEP, KWAVELOC, CGLOC
      RETURN
      END SUBROUTINE SWAPAR1


      SUBROUTINE SPROXY(I1, ISS, IDD, CANX, CANY, CGLOC, DIR2, DIR3,
     &                  UIJ, VIJ)
!     Dummy stub: wave coupling not active in this build.
      INTEGER I1, ISS, IDD
      REAL    CANX, CANY, CGLOC, DIR2, DIR3, UIJ, VIJ
      RETURN
      END SUBROUTINE SPROXY
```

Note the continuation line with `&` in column 6 (classic fixed-form style).

---

## 4. Building FVCOM

From `~/Desktop/FVCOM/src`:

```bash
# clean old objects and modules
make clean

# rebuild (single core while debugging)
make fvcom -j1
```

If it succeeds, copy the executable to the run directory:

```bash
cp fvcom ../run/
```

---

## 5. Running a simple test (single core)

From `~/Desktop/FVCOM/run`:

- Ensure `tst_run.nml` is present and consistent with compile flags. Key bits:

  ```fortran
  &NML_STARTUP
    STARTUP_TYPE = 'coldstart',
    STARTUP_FILE = 'tst_its.nc',
    ...
  /

  &NML_INTEGRATION
    EXTSTEP_SECONDS = 1.0e-4,
    ISPLIT          = 10,
    ...
  /

  &NML_NETCDF
    NC_ON          = T,
    NC_FIRST_OUT   = 'cycles=0',
    NC_OUT_INTERVAL= 'cycles=100',
    ...
  /

  &NML_PHYSICS
    TEMPERATURE_ACTIVE = T,
    SALINITY_ACTIVE    = T,
    WETTING_DRYING_ON  = T,         ! matches -DWET_DRY
    ...
  /

  &NML_GRID_COORDINATES
    GRID_FILE          = 'tst_grd.dat',
    GRID_FILE_UNITS    = 'meters',
    PROJECTION_REFERENCE = 'none',  ! cartesian test grid
    SIGMA_LEVELS_FILE  = 'tst_sigma.dat',
    DEPTH_FILE         = 'tst_dep.dat',
    ...
  /
  ```

- Input files should be under `INPDIR/` (e.g. `INPDIR/tst_its.nc`, `INPDIR/tst_sigma.dat`, etc.).

Run:

```bash
./fvcom --casename=tst --dbg=7 > fvcom_tst.log 2>&1
tail -n 120 fvcom_tst.log
```

You should see:

- no “Fatal Error!” messages
- NetCDF file(s) in `OUTDIR/` such as `OUTDIR/tst_0001.nc`.

Quick sanity check:

```bash
ncdump -h OUTDIR/tst_0001.nc | head
ncdump -v zeta OUTDIR/tst_0001.nc | head
```

---

## 6. Running on multiple cores

Still in `run/`, with MPI installed:

```bash
mpirun -np 4 ./fvcom --casename=tst --dbg=7 > fvcom_tst_4core.log 2>&1
```

Check that:

- run completes without MPI abort,
- `nprocs` variable in the NetCDF output equals the number of ranks,
- results are reasonable (surface elevation, etc.).

---

## 7. Things that *are not* compile problems

If you see FVCOM messages like:

- `Can Not Read NameList NML_...`
- `FILE ./INPDIR/none NOT FOUND`
- `BOTTOM_ROUGHNESS_MINIMUM outside valid range`
- `Improper formatting of GRID FILE: ISCAN ERROR`
- `You must specify a valid projection reference`

those are **runtime configuration / input file issues**, not build issues.

Fix them by editing:

- `tst_run.nml`
- grid / sigma / depth / forcing files

rather than touching the code or make system.

---

**TL;DR:**  
Once `make.inc` and `makefile` are set up as above and the two stub files (`stubs_icing_upcase.F`, `stubs_wavecomm.F`) exist and are clean, FVCOM builds and runs **without** the ice and wave modules and without any link-time drama.
