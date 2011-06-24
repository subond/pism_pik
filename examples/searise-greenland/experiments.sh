#!/bin/bash

# Copyright (C) 2009-2011 The PISM Authors

# PISM SeaRISE Greenland experiments

# Before using this script, run preprocess.sh and then initialization.sh
# recommended way to run with N processors is
#    "./experiments.sh N foo.nc >& out.experiments &"
# to initialize from spinup result foo.nc

echo
echo "# ============================================================================="
echo "# PISM SeaRISE Greenland: experiment runs"
echo "# ============================================================================="
echo

set -e  # exit on error

if [ -n "${SCRIPTNAME:+1}" ] ; then
  echo "[SCRIPTNAME=$SCRIPTNAME (already set)]"
  echo ""
else
  SCRIPTNAME="#(experiments.sh)"
fi

NN=2  # default number of processors
if [ $# -gt 0 ] ; then
  NN="$1"
fi

SPINUPRESULT=g10km_0_ftt.nc  # default name of spinup result
if [ $# -gt 1 ] ; then
  SPINUPRESULT="$2"
fi

PISM_CONFIG=searise_config.nc

for INPUT in $SPINUPRESULT $PISM_CONFIG; do
  if [ -e "$INPUT" ] ; then  # check if file exist
    echo "$SCRIPTNAME INPUT   $INPUT FOUND"
  else
    echo "$SCRIPTNAME INPUT   $INPUT MISSING!!!"
    echo
    echo "$SCRIPTNAME !!!!   RUN  spinup.sh  TO GENERATE  $INPUT   !!!!"
    echo
  fi
done
echo

echo "$SCRIPTNAME              NN = $NN"

# set MPIDO if using different MPI execution command, for example:
#  $ export PISM_MPIDO="aprun -n "
if [ -n "${PISM_MPIDO:+1}" ] ; then  # check if env var is already set
  echo "$SCRIPTNAME      PISM_MPIDO = $PISM_MPIDO  (already set)"
else
  PISM_MPIDO="mpiexec -n "
  echo "$SCRIPTNAME      PISM_MPIDO = $PISM_MPIDO"
fi

# check if env var PISM_DO was set (i.e. PISM_DO=echo for a 'dry' run)
if [ -n "${PISM_DO:+1}" ] ; then  # check if env var DO is already set
  echo "$SCRIPTNAME         PISM_DO = $PISM_DO  (already set)"
else
  PISM_DO="" 
fi

# prefix to pism (not to executables)
if [ -n "${PISM_PREFIX:+1}" ] ; then  # check if env var is already set
  echo "$SCRIPTNAME     PISM_PREFIX = $PISM_PREFIX  (already set)"
else
  PISM_PREFIX=""    # just a guess
  echo "$SCRIPTNAME     PISM_PREFIX = $PISM_PREFIX"
fi

# set PISM_EXEC if using different executables, for example:
#  $ export PISM_EXEC="pismr -cold"
if [ -n "${PISM_EXEC:+1}" ] ; then  # check if env var is already set
  echo "$SCRIPTNAME       PISM_EXEC = $PISM_EXEC  (already set)"
else
  PISM_EXEC="pismr"
  echo "$SCRIPTNAME       PISM_EXEC = $PISM_EXEC"
fi

# all runs will use  -skip $PISM_SKIP
if [ -n "${PISM_SKIP:+1}" ] ; then
  echo "$SCRIPTNAME       PISM_SKIP = $PISM_SKIP (already set)"
else
  PISM_SKIP=200
  echo "$SCRIPTNAME       PISM_SKIP = $PISM_SKIP"
fi
SKIP=$PISM_SKIP

# output file names will be ...UAF1_G_D3_C1_E0..., etc. if PISM_INITIALS=UAF1
if [ -n "${PISM_INITIALS:+1}" ] ; then
  echo "$SCRIPTNAME   PISM_INITIALS = $PISM_INITIALS (already set)"
else
  PISM_INITIALS=UAF1
  echo "$SCRIPTNAME   PISM_INITIALS = $PISM_INITIALS"
fi
INITIALS=$PISM_INITIALS 

echo


# default choices in parameter study; see Bueler & Brown (2009) re "tillphi"
TILLPHI="-topg_to_phi 5.0,20.0,-300.0,700.0,10.0"

FULLPHYS="-ssa_sliding -thk_eff $TILLPHI"

# cat prefix and exec together
PISM="${PISM_PREFIX}${PISM_EXEC} -ocean_kill -config_override $PISM_CONFIG $FULLPHYS"

echo "$SCRIPTNAME         tillphi = '$TILLPHI'"
echo "$SCRIPTNAME    full physics = '$FULLPHYS'"
echo "$SCRIPTNAME      executable = '$PISM'"


# coupler settings
COUPLER_CTRL="-ocean constant -atmosphere searise_greenland -surface pdd"
# coupler settings for spin-up (i.e. with forcing)
COUPLER_AR4="-ocean constant -atmosphere searise_greenland,anomaly -surface pdd"

echo
echo "$SCRIPTNAME control coupler = '$COUPLER_CTRL'"
echo "$SCRIPTNAME     AR4 coupler = '$COUPLER_AR4'"

expackage="-extra_vars usurf,topg,thk,bmelt,bwat,bwp,dHdt,mask,velsurf,wvelsurf,velbase,wvelbase,tempsurf,tempbase,diffusivity,acab,cbase,csurf"
tspackage="-ts_vars ivol,iareag,iareaf"

STARTTIME=0
ENDTIME=500

TIMES=${STARTTIME}:5:${ENDTIME}
TSTIMES=${STARTTIME}:1:${ENDTIME}

INNAME=${SPINUPRESULT}

# #######################################
# Control Run
# #######################################

CLIMATE=1
SRGEXPERCATEGORY=E0
PISM_SRPREFIX2=${INITIALS}_G_D3_C${CLIMATE}_${SRGEXPERCATEGORY}
OUTNAME=out_y${ENDTIME}_${PISM_SRPREFIX2}.nc
EXNAME=${PISM_SRPREFIX2}_raw_y${ENDTIME}.nc
TSNAME=ts_y${ENDTIME}_${PISM_SRPREFIX2}.nc
echo
echo "$SCRIPTNAME control run with steady climate from $STARTTIME to $ENDTIME years w save every 5 years:"
echo
cmd="$PISM_MPIDO $NN $PISM -skip $SKIP -i $INNAME $COUPLER_CTRL -ys $STARTTIME -ye $ENDTIME -o $OUTNAME \
  -extra_file $EXNAME -extra_times $TIMES $expackage \
  -ts_file $TSNAME -ts_times $TSTIMES $tspackage"
$PISM_DO $cmd
echo
echo "$SCRIPTNAME  $SRGNAME run with constant climate done; runs ...$PISM_SRPREFIX2... will need post-processing"
echo

# #######################################
# Climate Experiments C
# #######################################

CLIMATE=2
SRGEXPERCATEGORY=E0
for climate_scale_factor in 1.0 1.5 2.0; do


    # anomaly files
    AR4PRECIP=ar4_precip_anomaly_scalefactor_${climate_scale_factor}.nc
    AR4TEMP=ar4_temp_anomaly_scalefactor_${climate_scale_factor}.nc
    for INPUT in $AR4PRECIP $AR4TEMP; do
        if [ -e "$INPUT" ] ; then  # check if file exist
        echo "$SCRIPTNAME INPUT   $INPUT FOUND"
        else
            echo "$SCRIPTNAME INPUT   $INPUT MISSING!!!"
            echo
            echo "$SCRIPTNAME !!!!     please run preprocess.sh    !!!!"
            echo
        fi
    done
    
    PISM_SRPREFIX2=${INITIALS}_G_D3_C${CLIMATE}_${SRGEXPERCATEGORY}
    OUTNAME=out_y${ENDTIME}_${PISM_SRPREFIX2}.nc
    EXNAME=${PISM_SRPREFIX2}_raw_y${ENDTIME}.nc
    TSNAME=ts_y${ENDTIME}_${PISM_SRPREFIX2}.nc
    echo
    echo "$SCRIPTNAME run ${PISM_SRPREFIX2} with scaled AR4 climate from $STARTTIME to $ENDTIME years w save every 5 years:"
    echo
    cmd="$PISM_MPIDO $NN $PISM -skip $SKIP -i $INNAME $COUPLER_AR4 -ys $STARTTIME -ye $ENDTIME -o $OUTNAME \
       -anomaly_temp $AR4TEMP -anomaly_precip $AR4PRECIP \
       -extra_file $EXNAME -extra_times $TIMES $expackage \
       -ts_file $TSNAME -ts_times $TSTIMES $tspackage"
    $PISM_DO $cmd
    echo
    echo "$SCRIPTNAME  $SRGNAME run with AR4 climate done; runs ...$PISM_SRPREFIX2... will need post-processing"
    echo

    CLIMATE=$(($CLIMATE + 1))

done

# #######################################
# Sliding Experiments S
# #######################################

CLIMATE=1
SLIDING=1
for sliding_scale_factor in 2 2.5 3; do


  SRGEXPERCATEGORY="S$SLIDING"

  PISM_SRPREFIX1=${INITIALS}_G_D3_C${CLIMATE}_${SRGEXPERCATEGORY}
  OUTNAME=out_y${ENDTIME}_${PISM_SRPREFIX1}.nc
  EXNAME=${PISM_SRPREFIX1}_raw_y${ENDTIME}.nc
  TSNAME=ts_y${ENDTIME}_${PISM_SRPREFIX1}.nc
  echo
  echo "$SCRIPTNAME run ${PISM_SRPREFIX1} with steady climate from $STARTTIME to $ENDTIME years w save every 5 years:"
  echo
  cmd="$PISM_MPIDO $NN $PISM -skip $SKIP -i $INNAME $COUPLER_CTRL -ys $STARTTIME -ye $ENDTIME -o $OUTNAME \
  -sliding_scale $sliding_scale_factor \
  -extra_file $EXNAME -extra_times $TIMES $expackage \
  -ts_file $TSNAME -ts_times $TSTIMES $tspackage "
  $PISM_DO $cmd
  echo
  echo "$SCRIPTNAME  $SRGNAME run with steady climate done; results ...$PISM_SRPREFIX1... will need post-processing"
  echo

  SLIDING=$(($SLIDING + 1))

done

# #######################################
# Melt Rate Experiments M
# #######################################

CLIMATE=1
MELTRATE=1
for melt_rate in 2 20 200; do


  SRGEXPERCATEGORY="M$MELTRATE"

  PISM_SRPREFIX1=${INITIALS}_G_D3_C${CLIMATE}_${SRGEXPERCATEGORY}
  OUTNAME=out_y${ENDTIME}_${PISM_SRPREFIX1}.nc
  EXNAME=${PISM_SRPREFIX1}_raw_y${ENDTIME}.nc
  TSNAME=ts_y${ENDTIME}_${PISM_SRPREFIX1}.nc
  echo
  echo "$SCRIPTNAME run  ${PISM_SRPREFIX1} with steady climate from $STARTTIME to $ENDTIME years w save every 5 years:"
  echo
  cmd="$PISM_MPIDO $NN $PISM -skip $SKIP -i $INNAME $COUPLER_CTRL -ys $STARTTIME -ye $ENDTIME -o $OUTNAME \
  -shelf_base_melt_rate $melt_rate \
  -extra_file $EXNAME -extra_times $TIMES $expackage \
  -ts_file $TSNAME -ts_times $TSTIMES $tspackage "
  $PISM_DO $cmd
  echo
  echo "$SCRIPTNAME  $SRGNAME run with steady climate done; results ...$PISM_SRPREFIX1... will need post-processing"
  echo

  MELTRATE=$(($MELTRATE + 1))

done
