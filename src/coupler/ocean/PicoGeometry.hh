/* Copyright (C) 2018 PISM Authors
 *
 * This file is part of PISM.
 *
 * PISM is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * PISM is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PISM; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef PICOGEOMETRY_H
#define PICOGEOMETRY_H

#include "pism/util/iceModelVec.hh"
#include "pism/util/Component.hh"

namespace pism {

class IceModelVec2S;
class IceModelVec2CellType;

namespace ocean {

/*!
 * This class isolates geometric computations performed by the PICO ocean model.
 */
class PicoGeometry : public Component {
public:
  PicoGeometry(IceGrid::ConstPtr grid);
  virtual ~PicoGeometry();

  void update(const IceModelVec2S &bed_elevation,
              const IceModelVec2CellType &cell_type);

  const IceModelVec2Int& continental_shelf_mask() const;
  const IceModelVec2Int& box_mask() const;
  const IceModelVec2Int& ice_shelf_mask() const;

  enum IceRiseMask {OCEAN = 0, RISE = 1, CONTINENTAL = 2, FLOATING = 3};

  void compute_ice_rises(const IceModelVec2CellType &cell_type, IceModelVec2Int &result);
  void compute_lakes(const IceModelVec2CellType &cell_type, IceModelVec2Int &result);
  void compute_continental_shelf_mask(const IceModelVec2S &bed_elevation,
                                      const IceModelVec2Int &ice_rises_mask,
                                      double bed_elevation_threshold,
                                      IceModelVec2Int &result);
  void compute_ice_shelf_mask(const IceModelVec2Int &ice_rises_mask,
                              IceModelVec2Int &result);
private:
  void label_tmp();
  void relabel_by_size(IceModelVec2Int &mask);

  IceModelVec2Int m_continental_shelf;
  IceModelVec2Int m_boxes;
  IceModelVec2Int m_ice_shelves;

  IceModelVec2Int m_ice_rises;
  IceModelVec2Int m_tmp;
  petsc::Vec::Ptr m_tmp_p0;
};

} // end of namespace ocean
} // end of namespace pism

#endif /* PICOGEOMETRY_H */