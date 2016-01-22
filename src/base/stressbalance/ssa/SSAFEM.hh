// Copyright (C) 2009--2016 Jed Brown and Ed Bueler and Constantine Khroulev and David Maxwell
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

#ifndef _SSAFEM_H_
#define _SSAFEM_H_

#include "SSA.hh"
#include "FETools.hh"
#include "base/util/petscwrappers/SNES.hh"
#include "base/util/TerminationReason.hh"

namespace pism {
namespace stressbalance {

//! Factory function for constructing a new SSAFEM.
SSA * SSAFEMFactory(IceGrid::ConstPtr , EnthalpyConverter::Ptr);

//! PISM's SSA solver: the finite element method implementation written by Jed and David
/*!
  Jed's original code is in rev 831: src/base/ssaJed/...
  The SSAFEM duplicates most of the functionality of SSAFD, using the finite element method.
*/
class SSAFEM : public SSA
{
public:
  SSAFEM(IceGrid::ConstPtr g, EnthalpyConverter::Ptr e);

  virtual ~SSAFEM();

protected:
  virtual void init_impl();
  void cache_inputs();

  //! Storage for SSA coefficients at a quadrature point.
  struct Coefficients {
    //! ice thickness
    double H;
    //! basal yield stress
    double tauc;
    //! bed elevation
    double b;
    //! ice hardness
    double B;
    //! prescribed gravitational driving stress
    Vector2 driving_stress;
    //! mask used to choose basal conditions
    int mask;
  };

  void PointwiseNuHAndBeta(const Coefficients &coefficients,
                           const Vector2 &u, const double Du[],
                           double *nuH, double *dnuH,
                           double *beta, double *dbeta);

  void compute_local_function(Vector2 const *const *const velocity,
                              Vector2 **residual);

  void compute_local_jacobian(Vector2 const *const *const velocity, Mat J);

  virtual void solve();

  TerminationReason::Ptr solve_with_reason();

  TerminationReason::Ptr solve_nocache();

  //! Adaptor for gluing SNESDAFormFunction callbacks to an SSAFEM.
  /* The callbacks from SNES are mediated via SNESDAFormFunction, which has the
     convention that its context argument is a pointer to a struct
     having a DA as its first entry.  The CallbackData fulfills
     this requirement, and allows for passing the callback on to an honest
     SSAFEM object. */
  struct CallbackData {
    DM da;
    SSAFEM *ssa;
  };

  // objects used internally
  CallbackData m_callback_data;

  petsc::SNES m_snes;
  std::vector<Coefficients> m_coefficients;
  double m_dirichletScale;
  double m_ocean_rho;
  double m_beta_ice_free_bedrock;
  double m_epsilon_ssa;

  fem::ElementMap m_element_index;
  fem::Quadrature_Scalar m_quadrature;
  fem::Quadrature_Vector m_quadrature_vector;
  fem::DOFMap m_dofmap;

private:
  void monitor_jacobian(Mat Jac);
  void monitor_function(Vector2 const *const *const velocity_global,
                        Vector2 const *const *const residual_global);

  //! SNES callbacks.
  /*! These simply forward the call on to the SSAFEM member of the CallbackData */
static PetscErrorCode function_callback(DMDALocalInfo *info,
                                        Vector2 const *const *const velocity,
                                        Vector2 **residual,
                                        CallbackData *fe);
#if PETSC_VERSION_LT(3,5,0)
  static PetscErrorCode jacobian_callback(DMDALocalInfo *info,
                                          Vector2 const *const *const xg,
                                          Mat A, Mat J, MatStructure *str,
                                          CallbackData *fe);
#else
  static PetscErrorCode jacobian_callback(DMDALocalInfo *info,
                                          Vector2 const *const *const xg,
                                          Mat A, Mat J, CallbackData *fe);
#endif


};


} // end of namespace stressbalance
} // end of namespace pism

#endif /* _SSAFEM_H_ */
