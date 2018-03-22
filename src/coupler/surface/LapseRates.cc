// Copyright (C) 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018 PISM Authors
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

#include "LapseRates.hh"
#include "pism/util/io/io_helpers.hh"
#include "pism/util/pism_utilities.hh"
#include "pism/geometry/Geometry.hh"

namespace pism {
namespace surface {

LapseRates::LapseRates(IceGrid::ConstPtr g, std::shared_ptr<SurfaceModel> in)
  : PLapseRates<SurfaceModel,SurfaceModel>(g, in) {
  m_smb_lapse_rate = 0;
  m_option_prefix = "-surface_lapse_rate";

  m_mass_flux   = allocate_mass_flux(g);
  m_temperature = allocate_temperature(g);
}

LapseRates::~LapseRates() {
  // empty
}

void LapseRates::init_impl(const Geometry &geometry) {
  m_input_model->init(geometry);

  m_log->message(2,
             "  [using temperature and mass balance lapse corrections]\n");

  init_internal();

  m_smb_lapse_rate = options::Real("-smb_lapse_rate",
                                   "Elevation lapse rate for the surface mass balance,"
                                   " in m year-1 per km",
                                   m_smb_lapse_rate);

  m_log->message(2,
             "   ice upper-surface temperature lapse rate: %3.3f K per km\n"
             "   ice-equivalent surface mass balance lapse rate: %3.3f m year-1 per km\n",
             m_temp_lapse_rate, m_smb_lapse_rate);

  m_temp_lapse_rate = units::convert(m_sys, m_temp_lapse_rate, "K/km", "K/m");

  // convert from [m year-1 / km] to [kg m-2 year-1 / km]
  m_smb_lapse_rate *= m_config->get_double("constants.ice.density");
  m_smb_lapse_rate = units::convert(m_sys, m_smb_lapse_rate,
                                    "(kg m-2) year-1 / km", "(kg m-2) second-1 / m");
}

void LapseRates::update_impl(const Geometry &geometry, double t, double dt) {
  super::update_impl(geometry, t, dt);

  const IceModelVec2S &surface = geometry.ice_surface_elevation;

  m_mass_flux->copy_from(m_input_model->mass_flux());
  lapse_rate_correction(surface, m_reference_surface,
                        m_smb_lapse_rate, *m_mass_flux);

  m_temperature->copy_from(m_input_model->temperature());
  lapse_rate_correction(surface, m_reference_surface,
                        m_temp_lapse_rate, *m_temperature);
}

const IceModelVec2S &LapseRates::mass_flux_impl() const {
  return *m_mass_flux;
}

const IceModelVec2S &LapseRates::temperature_impl() const {
  return *m_temperature;
}


} // end of namespace surface
} // end of namespace pism
