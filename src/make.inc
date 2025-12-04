# ================================
# FVCOM make.inc for Ubuntu/WSL
# gfortran + mpif90 build
# ================================

# ---- TOPDIR (edit this!) ----
TOPDIR = /mnt/c/Users/jinikeda/Desktop/FVCOM

# ---- Install external libraries here (unused) ----
INSTALLDIR = $(TOPDIR)/libs/install

# ---- Preprocessor ----
DEF_FLAGS = -P -traditional

# ---- SYSTEM NetCDF ----
FLAG_USE_NETCDF4 = -DUSE_NETCDF4
IOLIBS  = -lnetcdff -lnetcdf -lmetis
IOINCS  = -I/usr/include

# ---- No external julian library needed ----
DTLIBS  = -L$(TOPDIR)/libs/install/lib -ljulian
DTINCS  = -I$(TOPDIR)/libs/install/include

# ===========================
# MODEL FLAGS (keep defaults)
# ===========================

FLAG_1   = -DDOUBLE_PRECISION -DSINGLE_OUTPUT
FLAG_2   = 
FLAG_3   = -DWET_DRY
FLAG_4   = -DMULTIPROCESSOR
# Disable METIS special partition
FLAG_411 =
PARLIB   =
PARTLIBS =
PARTINCS =

FLAG_8   = -DLIMITED_NO
FLAG_10  = -DGCN
FLAG_44  = -DTVD
FLAG_15  = -DMPDATA

# ---- Disable all advanced modules ----
FLAG_5  = -DICE
FLAG_6  =
FLAG_7  =
FLAG_9  = 
FLAG_11 =
FLAG_12 =
FLAG_13 =
FLAG_14 =
FLAG_16 =
FLAG_17 =
FLAG_18 =
FLAG_19 =
FLAG_20 =
FLAG_21 =
FLAG_22 =
FLAG_23 =
FLAG_24 =
FLAG_25 =
FLAG_26 = 
FLAG_27 =
FLAG_28 =
FLAG_29 =
FLAG_30 =
FLAG_31 =
FLAG_32 = -DWAVE_CURRENT_INTERACTION
FLAG_33 =
FLAG_34 = -DEXPLICIT
FLAG_35 =
FLAG_36 =
FLAG_37 =
FLAG_38 =
FLAG_39 =
FLAG_40 =
FLAG_41 = -DPETSC_C
FLAG_42 =
FLAG_43 =
FLAG_45 =
FLAG_80 =
FLAG_81 =
FLAG_101 =
FLAG_102 =
FLAG_103 =
FLAG_104 =
FLAG_105 =
FLAG_321 =
FLAG_211 =
FLAG_212 =
FLAG_213 =
FLAG_251 =
FLAG_261 = 
FLAG_262 = 

# ===========================
# COMPILER SETTINGS
# ===========================

CPP      = /usr/bin/cpp
COMPFLAG = -DGFORTRAN

FC       = mpif90
CC       = mpicc
CXX      = mpicxx

DEBFLGS  = -g -fbacktrace -fcheck=all -ffpe-trap=invalid,zero,overflow
OPT      = -O0
CFLAGS   = -O3

CPPFLAGS = $(DEF_FLAGS) $(COMPFLAG) $(INCS)
FFLAGS   = $(DEBFLGS) $(OPT) $(INCS)

MDEPFLAGS = --cpp --fext=f90 --file=-

RANLIB = ranlib
AR = ar rc

# ===========================
# INCLUDE & LIBRARY PATHS
# ===========================

INCDIR     = -I/usr/include
LIBDIR     = -L/usr/lib/x86_64-linux-gnu
PETSC_DIR  = /home/jinikeda/petsc
PETSC_ARCH = arch-linux-c-opt

# ===========================
# BUILD FLAGS
# ===========================

# Load PETSc build settings (defines PETSC_FC_INCLUDES, PETSC_LIB, AR, etc.)
include ${PETSC_DIR}/lib/petsc/conf/variables

CPPARGS = $(CPPFLAGS) $(DEF_FLAGS) \
  $(FLAG_1)  $(FLAG_3)  $(FLAG_4)  $(FLAG_5) \
  $(FLAG_6)  $(FLAG_8)  $(FLAG_10) $(FLAG_14) \
  $(FLAG_21) $(FLAG_28) \
  $(FLAG_32) $(FLAG_33) $(FLAG_34) \
  $(FLAG_41) $(FLAG_411) \
  $(FLAG_USE_NETCDF4)

LIBS = $(LIBDIR) $(PARLIB) $(PARTLIBS) $(IOLIBS) $(DTLIBS) \
       $(PROJLIBS) $(PETSC_LIB)

INCS = $(INCDIR) $(IOINCS) $(GOTMINCS) $(BIOINCS) \
       $(VISITINCPATH) $(PROJINCS) $(DTINCS) $(PARTINCS) \
       $(PETSC_FC_INCLUDES)

# Restore the AR/RANLIB that FVCOM expects (PETSc changed them)
AR     = ar rc
RANLIB = ranlib