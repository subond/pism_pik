// Copyright (C) 2008-2018 PISM Authors
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
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

#include <gsl/gsl_math.h>

#include "ConstantPIK.hh"
#include "pism/util/io/PIO.hh"
#include "pism/util/Vars.hh"
#include "pism/util/IceGrid.hh"
#include "pism/util/pism_utilities.hh"
#include "pism/util/MaxTimestep.hh"
#include "pism/geometry/Geometry.hh"

namespace pism {
namespace surface {

///// Constant-in-time surface model for accumulation,
///// ice surface temperature parameterized as in PISM-PIK dependent on latitude and surface elevation


PIK::PIK(IceGrid::ConstPtr g, std::shared_ptr<atmosphere::AtmosphereModel> atmosphere)
  : SurfaceModel(g) {
  (void) atmosphere;

  m_mass_flux.create(m_grid, "climatic_mass_balance", WITHOUT_GHOSTS);
  m_mass_flux.set_attrs("climate_state",
                                  "constant-in-time surface mass balance (accumulation/ablation) rate",
                                  "kg m-2 s-1",
                                  "land_ice_surface_specific_mass_balance_flux");
  m_mass_flux.metadata().set_string("glaciological_units", "kg m-2 year-1");

  m_temperature.create(m_grid, "ice_surface_temp", WITHOUT_GHOSTS);
  m_temperature.set_attrs("climate_state",
                             "constant-in-time ice temperature at the ice surface",
                             "K", "");
}

void PIK::init_impl(const Geometry &geometry) {
  (void) geometry;

  m_log->message(2,
                 "* Initializing the constant-in-time surface processes model PIK.\n"
                 "  It reads surface mass balance directly from the file and holds it constant.\n"
                 "  Ice upper-surface temperature is parameterized as in Martin et al. 2011, Eqn. 2.0.2.\n"
                 "  Any choice of atmosphere coupler (option '-atmosphere') is ignored.\n");

  InputOptions opts = process_input_options(m_grid->com);

  // read snow precipitation rate from file
  m_log->message(2,
                 "    reading surface mass balance rate 'climatic_mass_balance' from %s ... \n",
                 opts.filename.c_str());
  if (opts.type == INIT_BOOTSTRAP) {
    m_mass_flux.regrid(opts.filename, CRITICAL); // fails if not found!
  } else {
    m_mass_flux.read(opts.filename, opts.record); // fails if not found!
  }

  // parameterizing the ice surface temperature 'ice_surface_temp'
  m_log->message(2,
                 "    parameterizing the ice surface temperature 'ice_surface_temp' ... \n");
}

MaxTimestep PIK::max_timestep_impl(double t) const {
  (void) t;
  return MaxTimestep("surface PIK");
}

void PIK::update_impl(const Geometry &geometry, double t, double dt) {
  if ((fabs(t - m_t) < 1e-12) and (fabs(dt - m_dt) < 1e-12)) {
    return;
  }

  m_t  = t;
  m_dt = dt;

  const IceModelVec2S
    &usurf = geometry.ice_surface_elevation,
    &lat   = geometry.latitude;

  IceModelVec::AccessList list{ &m_temperature, &usurf, &lat };

  for (Points p(*m_grid); p; p.next()) {
    const int i = p.i(), j   = p.j();
    m_temperature(i, j) = 273.15 + 30 - 0.0075 * usurf(i, j) - 0.68775 * lat(i, j) * (-1.0);
  }
}

const IceModelVec2S &PIK::mass_flux_impl() const {
  return m_mass_flux;
}

const IceModelVec2S &PIK::temperature_impl() const {
  return m_temperature;
}

void PIK::define_model_state_impl(const PIO &output) const {
  m_mass_flux.define(output);
  SurfaceModel::define_model_state_impl(output);
}

void PIK::write_model_state_impl(const PIO &output) const {
  m_mass_flux.write(output);
  SurfaceModel::write_model_state_impl(output);
}

} // end of namespace surface
} // end of namespace pism
