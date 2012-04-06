// Copyright (C) 2009, 2010, 2011, 2012 Ed Bueler and Constantine Khroulev
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2 of the License, or (at your option) any later
// version.
//
// PISM is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License
// along with PISM; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

static char help[] = 
  "Driver for testing PISM's boundary (surface and shelf-base) models without IceModel.\n";

#include <set>
#include <ctime>
#include <string>
#include <sstream>
#include <vector>
#include <petscdmda.h>
#include "pism_const.hh"
#include "pism_options.hh"
#include "IceGrid.hh"
#include "LocalInterpCtx.hh"
#include "PIO.hh"
#include "NCVariable.hh"
#include "Timeseries.hh"

#include "PAFactory.hh"
#include "POFactory.hh"
#include "PSFactory.hh"
#include "PISMVars.hh"
#include "PISMTime.hh"


static PetscErrorCode setupIceGridFromFile(string filename, IceGrid &grid) {
  PetscErrorCode ierr;

  PIO nc(grid.com, grid.rank, "netcdf3");

  ierr = nc.open(filename, PISM_NOWRITE); CHKERRQ(ierr);
  ierr = nc.inq_grid("land_ice_thickness", &grid, NOT_PERIODIC); CHKERRQ(ierr);
  ierr = nc.close(); CHKERRQ(ierr);

  grid.compute_nprocs();
  grid.compute_ownership_ranges();
  ierr = grid.createDA(); CHKERRQ(ierr);
  return 0;
}

static PetscErrorCode createVecs(IceGrid &grid, PISMVars &variables) {
  
  PetscErrorCode ierr;
  IceModelVec2S *lat, *lon, *mask, *thk, *surfelev, *topg, *acab, *artm, *shelfbasetemp, *shelfbasemassflux;

  lat      = new IceModelVec2S;
  lon      = new IceModelVec2S;
  mask     = new IceModelVec2S;
  thk      = new IceModelVec2S;
  surfelev = new IceModelVec2S;
  topg     = new IceModelVec2S;
  
  // the following are allocated by the pclimate code, but may or may not
  //   actually be read by PISMAtmosphereModel *atmosphere and PISMOceanModel *ocean
  acab     = new IceModelVec2S;
  artm     = new IceModelVec2S;
  shelfbasetemp     = new IceModelVec2S;
  shelfbasemassflux = new IceModelVec2S;

  ierr = lat->create(grid, "lat", true); CHKERRQ(ierr);
  ierr = lat->set_attrs("mapping", "latitude", "degrees_north", "latitude"); CHKERRQ(ierr);
  ierr = variables.add(*lat); CHKERRQ(ierr);

  ierr = lon->create(grid, "lon", true); CHKERRQ(ierr);
  ierr = lon->set_attrs("mapping", "longitude", "degrees_east", "longitude"); CHKERRQ(ierr);
  ierr = variables.add(*lon); CHKERRQ(ierr);

  ierr = mask->create(grid, "mask", true); CHKERRQ(ierr);
  ierr = mask->set_attrs("", "grounded_dragging_floating integer mask",
			      "", ""); CHKERRQ(ierr);
  ierr = variables.add(*mask); CHKERRQ(ierr);

  ierr = thk->create(grid, "thk", true); CHKERRQ(ierr);
  ierr = thk->set_attrs("", "land ice thickness",
		             "m", "land_ice_thickness"); CHKERRQ(ierr);
  ierr = variables.add(*thk); CHKERRQ(ierr);

  ierr = surfelev->create(grid, "usurf", true); CHKERRQ(ierr);
  ierr = surfelev->set_attrs("", "ice upper surface elevation",
		                  "m", "surface_altitude"); CHKERRQ(ierr);
  ierr = variables.add(*surfelev); CHKERRQ(ierr);

  ierr = topg->create(grid, "topg", true); CHKERRQ(ierr);
  ierr = topg->set_attrs("", "bedrock surface elevation",
			"m", "bedrock_altitude"); CHKERRQ(ierr);
  ierr = variables.add(*topg); CHKERRQ(ierr);

  ierr = artm->create(grid, "artm", false); CHKERRQ(ierr);
  ierr = artm->set_attrs("climate_state",
			 "annual average ice surface temperature, below firn processes",
			 "K",
			 ""); CHKERRQ(ierr);
  ierr = variables.add(*artm); CHKERRQ(ierr);

  ierr = acab->create(grid, "acab", false); CHKERRQ(ierr);
  ierr = acab->set_attrs("climate_state", 
			 "ice-equivalent surface mass balance (accumulation/ablation) rate",
			 "m s-1", 
			 ""); CHKERRQ(ierr);
  ierr = acab->set_glaciological_units("m year-1"); CHKERRQ(ierr);
  acab->write_in_glaciological_units = true;
  ierr = variables.add(*acab); CHKERRQ(ierr);

  ierr = shelfbasetemp->create(grid, "shelfbtemp", false); CHKERRQ(ierr); // no ghosts; NO HOR. DIFF.!
  ierr = shelfbasetemp->set_attrs(
				 "climate_state", "absolute temperature at ice shelf base",
				 "K", ""); CHKERRQ(ierr);
  // PROPOSED standard name = ice_shelf_basal_temperature
  ierr = variables.add(*shelfbasetemp); CHKERRQ(ierr);

  // ice mass balance rate at the base of the ice shelf; sign convention for vshelfbasemass
  //   matches standard sign convention for basal melt rate of grounded ice
  ierr = shelfbasemassflux->create(grid, "shelfbmassflux", false); CHKERRQ(ierr); // no ghosts; NO HOR. DIFF.!
  ierr = shelfbasemassflux->set_attrs("climate_state",
				     "ice mass flux from ice shelf base (positive flux is loss from ice shelf)",
				     "m s-1", ""); CHKERRQ(ierr);
  // PROPOSED standard name = ice_shelf_basal_specific_mass_balance
  shelfbasemassflux->write_in_glaciological_units = true;
  ierr = shelfbasemassflux->set_glaciological_units("m year-1"); CHKERRQ(ierr);
  ierr = variables.add(*shelfbasemassflux); CHKERRQ(ierr);


  return 0;
}

static PetscErrorCode readIceInfoFromFile(string filename, int start,
                                          PISMVars &variables) {
  PetscErrorCode ierr;
  // Get the names of all the variables allocated earlier:
  set<string> vars = variables.keys();

  // Remove artm, acab, shelfbasemassflux and shelfbasetemp: they are filled by
  // surface and ocean models and aren't necessarily read from files.
  vars.erase("artm");
  vars.erase("acab");
  vars.erase("shelfbmassflux");
  vars.erase("shelfbtemp");

  set<string>::iterator i = vars.begin();
  while (i != vars.end()) {
    IceModelVec *var = variables.get(*i);
    ierr = var->read(filename, start); CHKERRQ(ierr);
    i++;
  }

  return 0;
}


static PetscErrorCode doneWithIceInfo(PISMVars &variables) {
  // Get the names of all the variables allocated earlier:
  set<string> vars = variables.keys();

  set<string>::iterator i = vars.begin();
  while (i != vars.end()) {
    IceModelVec *var = variables.get(*i);
    delete var;
    i++;
  }

  return 0;
}


static PetscErrorCode writePCCStateAtTimes(PISMVars &variables,
					   PISMSurfaceModel *surface,
					   PISMOceanModel* ocean,
					   const char *filename, IceGrid* grid,
					   PetscReal time_start, PetscReal time_end, PetscReal dt,
					   NCConfigVariable &mapping) {

  MPI_Comm com = grid->com;
  PetscErrorCode ierr;
  PIO nc(grid->com, grid->rank, grid->config.get_string("output_format"));
  NCGlobalAttributes global_attrs;
  IceModelVec2S *usurf, *artm, *acab, *shelfbasetemp, *shelfbasemassflux;

  usurf = dynamic_cast<IceModelVec2S*>(variables.get("surface_altitude"));
  if (usurf == NULL) { SETERRQ(com, 1, "usurf is not available"); }

  artm = dynamic_cast<IceModelVec2S*>(variables.get("artm"));
  if (artm == NULL) { SETERRQ(com, 1, "artm is not available"); }

  acab = dynamic_cast<IceModelVec2S*>(variables.get("acab"));
  if (acab == NULL) { SETERRQ(com, 1, "acab is not available"); }

  shelfbasetemp = dynamic_cast<IceModelVec2S*>(variables.get("shelfbtemp"));
  if (shelfbasetemp == NULL) { SETERRQ(com, 1, "shelfbasetemp is not available"); }

  shelfbasemassflux = dynamic_cast<IceModelVec2S*>(variables.get("shelfbmassflux"));
  if (shelfbasemassflux == NULL) { SETERRQ(com, 1, "shelfbasemassflux is not available"); }

  global_attrs.init("global_attributes", com, grid->rank);
  global_attrs.set_string("Conventions", "CF-1.4");
  global_attrs.set_string("source", string("pclimate ") + PISM_Revision);

  // Create a string with space-separated command-line arguments:
  string history = pism_username_prefix() + pism_args_string();

  global_attrs.prepend_history(history);

  ierr = nc.open(filename, PISM_WRITE); CHKERRQ(ierr);
  // append == false, check_dims == true
  ierr = nc.close(); CHKERRQ(ierr);

  ierr = mapping.write(filename); CHKERRQ(ierr);
  ierr = global_attrs.write(filename); CHKERRQ(ierr);

  PetscInt NN;  // get number of times at which PISM boundary model state is written
  NN = (int) ceil((time_end - time_start) / dt);
  if (NN > 1000)
    SETERRQ(com, 2,"PCLIMATE ERROR: refuse to write more than 1000 times!");
  if (NN > 50) {
    ierr = PetscPrintf(com,
        "\nPCLIMATE ATTENTION: writing more than 50 times to '%s'!!\n\n",
        filename); CHKERRQ(ierr);
  }

  DiagnosticTimeseries sea_level(grid, "sea_level", grid->config.get_string("time_dimension_name"));
  sea_level.set_units("m", "m");
  sea_level.set_dimension_units(grid->time->units(), "");
  sea_level.output_filename = filename;
  sea_level.set_attr("long_name", "sea level elevation");

  PetscScalar use_dt = dt;

  set<string> vars_to_write;
  surface->add_vars_to_output("big", vars_to_write);
  ocean->add_vars_to_output("big", vars_to_write);

  // write the states
  for (PetscInt k = 0; k < NN; k++) {
    // use original dt to get correct subinterval starts:
    const PetscReal time = time_start + k * dt;
    ierr = nc.open(filename, PISM_WRITE, true); CHKERRQ(ierr); // append=true,check_dims=false
    ierr = nc.def_time(grid->config.get_string("time_dimension_name"),
                       grid->config.get_string("calendar"),
                       grid->time->units()); CHKERRQ(ierr);
    ierr = nc.append_time(grid->config.get_string("time_dimension_name"),
                          time); CHKERRQ(ierr);

    PetscScalar dt_update = PetscMin(use_dt, time_end - time);

    char timestr[TEMPORARY_STRING_LENGTH];
    snprintf(timestr, sizeof(timestr), 
             "  boundary models updated for [%11.3f a,%11.3f a] ...", 
             convert(time, "seconds", "years"),
             convert(time + dt_update, "seconds", "years"));
    ierr = verbPrintf(2,com,"."); CHKERRQ(ierr);
    ierr = verbPrintf(3,com,"\n%s writing result to %s ..",timestr,filename); CHKERRQ(ierr);
    strncat(timestr,"\n",1);

    ierr = nc.append_history(timestr); CHKERRQ(ierr); // append the history
    ierr = nc.close(); CHKERRQ(ierr);

    ierr = usurf->write(filename, PISM_FLOAT); CHKERRQ(ierr);

    // update surface and ocean models' outputs:
    ierr = surface->update(time, dt_update); CHKERRQ(ierr);
    ierr = ocean->update(time, dt_update); CHKERRQ(ierr);

    ierr = surface->ice_surface_mass_flux(*acab); CHKERRQ(ierr);
    ierr = surface->ice_surface_temperature(*artm); CHKERRQ(ierr);

    PetscReal current_sea_level;
    ierr = ocean->sea_level_elevation(current_sea_level); CHKERRQ(ierr);

    ierr = ocean->shelf_base_temperature(*shelfbasetemp); CHKERRQ(ierr);
    ierr = ocean->shelf_base_mass_flux(*shelfbasemassflux); CHKERRQ(ierr);

    sea_level.append(current_sea_level, time - dt, time);
    sea_level.interp(time - dt, time);

    // ask ocean and surface models to write variables:
    ierr = surface->write_variables(vars_to_write, filename); CHKERRQ(ierr);
    ierr = ocean->write_variables(vars_to_write, filename); CHKERRQ(ierr);

    // This ensures that even if a surface model wrote artm and acab we
    // over-write them with values that were actually used by IceModel.
    ierr = acab->write(filename, PISM_FLOAT); CHKERRQ(ierr);
    ierr = artm->write(filename, PISM_FLOAT); CHKERRQ(ierr);

    // This ensures that even if a ocean model wrote shelfbasetemp and
    // shelfbasemassflux we over-write them with values that were actually used
    // by IceModel.
    ierr = shelfbasetemp->write(filename, PISM_FLOAT); CHKERRQ(ierr);
    ierr = shelfbasemassflux->write(filename, PISM_FLOAT); CHKERRQ(ierr);
  }
  ierr = verbPrintf(2,com,"\n"); CHKERRQ(ierr);

  return 0;
}


int main(int argc, char *argv[]) {
  PetscErrorCode  ierr;

  MPI_Comm    com;
  PetscMPIInt rank, size;

  ierr = PetscInitialize(&argc, &argv, PETSC_NULL, help); CHKERRQ(ierr);

  com = PETSC_COMM_WORLD;
  ierr = MPI_Comm_rank(com, &rank); CHKERRQ(ierr);
  ierr = MPI_Comm_size(com, &size); CHKERRQ(ierr);

  /* This explicit scoping forces destructors to be called before PetscFinalize() */
  {
    NCConfigVariable config, overrides, mapping;
    string inname, outname;

    ierr = verbosityLevelFromOptions(); CHKERRQ(ierr);

    ierr = verbPrintf(2,com,
      "PCLIMATE %s (surface and shelf-base boundary-models-only mode)\n",
      PISM_Revision); CHKERRQ(ierr);
    ierr = stop_on_version_option(); CHKERRQ(ierr);

    // check required options
    vector<string> required;
    required.push_back("-i");
    required.push_back("-o");
    required.push_back("-ys");
    required.push_back("-ye");
    required.push_back("-dt");
    ierr = show_usage_check_req_opts(com, "pclimate", required,
      "  pclimate -i IN.nc -o OUT.nc -ys A -ye B -dt C [-atmosphere <name> -surface <name>] [OTHER PISM & PETSc OPTIONS]\n"
      "where:\n"
      "  -i             input file in NetCDF format\n"
      "  -o             output file in NetCDF format\n"
      "  -ys            start time A (= float) in years\n"
      "  -ye            end time B (= float), B > A, in years\n"
      "  -dt            time step C (= positive float) in years\n"
      "and set up the models:\n"
      "  -atmosphere    Chooses an atmosphere model; see User's Manual\n"
      "  -surface       Chooses a surface model; see User's Manual\n"
      "  -ocean         Chooses an ocean model; see User's Manual\n"
      ); CHKERRQ(ierr);

    // read the config option database:
    ierr = init_config(com, rank, config, overrides, true); CHKERRQ(ierr);

    bool override_used;
    ierr = PISMOptionsIsSet("-config_override", override_used); CHKERRQ(ierr);

    // set an un-documented (!) flag to limit time-steps to 1 year.
    config.set_flag("pdd_limit_timestep", true);

    IceGrid grid(com, rank, size, config);
    
    bool flag;
    PetscReal dt_years = 0.0;
    ierr = PetscOptionsBegin(grid.com, "", "PCLIMATE options", ""); CHKERRQ(ierr);
    {
      ierr = PISMOptionsString("-i", "Input file name",  inname, flag); CHKERRQ(ierr);
      ierr = PISMOptionsString("-o", "Output file name", outname, flag); CHKERRQ(ierr);

      ierr = PISMOptionsReal("-dt", "Time-step, in years", dt_years, flag); CHKERRQ(ierr);
    }
    ierr = PetscOptionsEnd(); CHKERRQ(ierr);

    // initialize the computational grid:
    ierr = verbPrintf(2,com, 
		      "  initializing grid from NetCDF file %s...\n", inname.c_str()); CHKERRQ(ierr);
    ierr = setupIceGridFromFile(inname,grid); CHKERRQ(ierr);

    mapping.init("mapping", com, rank);

    // allocate IceModelVecs needed by boundary models and put them in a dictionary:
    PISMVars variables;
    ierr = createVecs(grid, variables); CHKERRQ(ierr);

    // read data from a PISM input file (including the projection parameters)
    PIO nc(grid.com, grid.rank, "netcdf3");
    unsigned int last_record;
    bool mapping_exists;

    ierr = nc.open(inname, PISM_NOWRITE); CHKERRQ(ierr);
    ierr = nc.inq_var("mapping", mapping_exists); CHKERRQ(ierr);
    ierr = nc.inq_nrecords(last_record); CHKERRQ(ierr);
    ierr = nc.close(); CHKERRQ(ierr);

    if (mapping_exists) {
      ierr = mapping.read(inname); CHKERRQ(ierr);
      ierr = mapping.print(); CHKERRQ(ierr);
    }
    last_record -= 1;

    ierr = verbPrintf(2,com,
                      "  reading fields lat,lon,mask,thk,topg,usurf from NetCDF file %s\n"
                      "    to fill fields in PISMVars ...\n",
		      inname.c_str()); CHKERRQ(ierr);

    ierr = readIceInfoFromFile(inname, last_record, variables); CHKERRQ(ierr);

    // Initialize boundary models:
    PAFactory pa(grid, config);
    PISMAtmosphereModel *atmosphere;

    PSFactory ps(grid, config);
    PISMSurfaceModel *surface;

    POFactory po(grid, config);
    PISMOceanModel *ocean;

    ierr = PetscOptionsBegin(grid.com, "", "PISM Boundary Models", ""); CHKERRQ(ierr);

    pa.create(atmosphere);
    ps.create(surface);
    po.create(ocean);

    surface->attach_atmosphere_model(atmosphere);
    ierr = surface->init(variables); CHKERRQ(ierr);
    ierr = ocean->init(variables); CHKERRQ(ierr);

    ierr = PetscOptionsEnd(); CHKERRQ(ierr);  // done initializing boundary models.

    ierr = verbPrintf(2, com,
        "writing boundary model states to NetCDF file '%s' ...\n",
        outname.c_str()); CHKERRQ(ierr);

    ierr = writePCCStateAtTimes(variables, surface, ocean,
                                outname.c_str(), &grid,
				grid.time->start(),
                                grid.time->end(),
                                convert(dt_years, "years", "seconds"),
                                mapping); CHKERRQ(ierr);

    if (override_used) {
      ierr = verbPrintf(3, com,
        "  recording config overrides in NetCDF file '%s' ...\n",
	outname.c_str()); CHKERRQ(ierr);
      overrides.update_from(config);
      ierr = overrides.write(outname.c_str()); CHKERRQ(ierr);
    }

    delete surface;
    delete ocean;
    ierr = doneWithIceInfo(variables); CHKERRQ(ierr);

    ierr = verbPrintf(2,com, "done.\n"); CHKERRQ(ierr);

  }

  ierr = PetscFinalize(); CHKERRQ(ierr);
  return 0;
}


