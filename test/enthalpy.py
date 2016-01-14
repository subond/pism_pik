"""Simple verification/regression tests for the PISM's enthalpy
solver. Work in progress.
"""

import PISM
import numpy as np
import pylab as plt

ctx = PISM.Context()
unit_system = ctx.unit_system
config = ctx.config

config.set_string("grid_ice_vertical_spacing_type", "equal")

k = config.get_double("ice_thermal_conductivity")
c = config.get_double("ice_specific_heat_capacity")
rho = config.get_double("ice_density")
K = k / c
# alpha squared
alpha2 = k / (c * rho)

EC = PISM.EnthalpyConverter(ctx.config)
pressure = np.vectorize(EC.pressure)
cts = np.vectorize(EC.enthalpy_cts)

class EnthalpyColumn(object):
    "Set up the grid and arrays needed to run column solvers"

    def __init__(self, Mz, dt):
        self.Lz = 1000.0
        self.z = np.linspace(0, self.Lz, Mz)

        param = PISM.GridParameters()
        param.Lx = 1e5
        param.Ly = 1e5
        param.z = PISM.DoubleVector(self.z)
        param.Mx = 3
        param.My = 3
        param.Mz = Mz

        param.ownership_ranges_from_options(1)

        self.dt = dt

        self.grid = PISM.IceGrid(ctx.ctx, param)
        grid = self.grid

        self.enthalpy = PISM.model.createEnthalpyVec(grid)

        self.strain_heating = PISM.model.createStrainHeatingVec(grid)

        self.u, self.v, self.w = PISM.model.create3DVelocityVecs(grid)

        self.sys = PISM.enthSystemCtx(grid.z(), "enth",
                                      grid.dx(), grid.dy(), self.dt,
                                      config,
                                      self.enthalpy,
                                      self.u, self.v, self.w,
                                      self.strain_heating,
                                      EC)

        # zero ice velocity:
        self.reset_flow()
        # no strain heating:
        self.reset_strain_heating()

    def reset_flow(self):
        self.u.set(0.0)
        self.v.set(0.0)
        self.w.set(0.0)

    def reset_strain_heating(self):
        self.strain_heating.set(0.0)

    def set_enthalpy(self, E):
        with PISM.vec.Access(nocomm=[self.enthalpy]):
            for j in [0, 1, 2]:
                for i in [0, 1, 2]:
                    for k, e in enumerate(E):
                        self.enthalpy[i,j,k] = e

    def init_column(self):
        ice_thickness = self.Lz
        marginal = False  # NOT marginal (but it does not matter)
        self.sys.init(1, 1, marginal, ice_thickness)

def convergence_rate_time(title, error_func):
    "Compute the convergence rate with refinement in time."
    dts = 2.0**np.arange(10)

    max_errors = np.zeros_like(dts)
    avg_errors = np.zeros_like(dts)
    for k, dt in enumerate(dts):
        max_errors[k], avg_errors[k] = error_func(False, dt_years=dt, Mz=101)

    log10 = np.log10

    p_max = np.polyfit(log10(dts), log10(max_errors), 1)
    p_avg = np.polyfit(log10(dts), log10(avg_errors), 1)

    if True:
        def log_plot(x, y, style, label):
            plt.plot(log10(x), log10(y), style, label=label)
            plt.xticks(log10(x), x)

        def log_fit_plot(x, p, label):
            plt.plot(log10(x), np.polyval(p, log10(x)), label=label)

        plt.figure()
        plt.title(title + "\nTesting convergence as dt -> 0")
        plt.hold(True)
        log_plot(dts, max_errors, 'o', "max errors")
        log_plot(dts, avg_errors, 'o', "avg errors")
        log_fit_plot(dts, p_max, "max: dt^{}".format(p_max[0]))
        log_fit_plot(dts, p_avg, "avg: dt^{}".format(p_avg[0]))
        plt.axis('tight')
        plt.grid(True)
        plt.legend(loc="best")
        plt.show()

    return p_max[0], p_avg[0]

def convergence_rate_space(title, error_func):
    "Compute the convergence rate with refinement in time."
    Mz = 2.0**np.arange(3,10)
    dzs = 1000.0 / Mz

    max_errors = np.zeros_like(dzs)
    avg_errors = np.zeros_like(dzs)
    for k, M in enumerate(Mz):
        T = 1.0
        max_errors[k], avg_errors[k] = error_func(False,
                                                  T_final_years=T,
                                                  dt_years=T,
                                                  Mz=M)

    log10 = np.log10

    p_max = np.polyfit(log10(dzs), log10(max_errors), 1)
    p_avg = np.polyfit(log10(dzs), log10(avg_errors), 1)

    if True:
        def log_plot(x, y, style, label):
            plt.plot(log10(x), log10(y), style, label=label)
            plt.xticks(log10(x), x)

        def log_fit_plot(x, p, label):
            plt.plot(log10(x), np.polyval(p, log10(x)), label=label)

        plt.figure()
        plt.title(title + "\nTesting convergence as dz -> 0")
        plt.hold(True)
        log_plot(dzs, max_errors, 'o', "max errors")
        log_plot(dzs, avg_errors, 'o', "avg errors")
        log_fit_plot(dzs, p_max, "max: dz^{}".format(p_max[0]))
        log_fit_plot(dzs, p_avg, "avg: dz^{}".format(p_avg[0]))
        plt.axis('tight')
        plt.grid(True)
        plt.legend(loc="best")
        plt.show()

    return p_max[0], p_avg[0]

def exact_DN(L, U0, QL):
    """Exact solution (and an initial state) for the 'Dirichlet at the base,
    Neumann at the top' setup."""
    n = 1
    lambda_n = 1.0 / L * (np.pi / 2.0 + n * np.pi)
    a = L * 25.0

    def f(z, t):
        v = a * np.exp(-lambda_n**2 * alpha2 * t) * np.sin(lambda_n * z)
        return v + (U0 + QL * z)
    return f

def errors_DN(plot_results=True, T_final_years=1000.0, dt_years=100, Mz=101):
    """Test the enthalpy solver with Dirichlet B.C. at the base and
    Neumann at the top surface.
    """
    T_final = PISM.convert(unit_system, T_final_years, "years", "seconds")
    dt = PISM.convert(unit_system, dt_years, "years", "seconds")

    column = EnthalpyColumn(Mz, dt)

    Lz = column.Lz
    z = np.array(column.sys.z())

    E_base = EC.enthalpy(230.0, 0.0, EC.pressure(Lz))
    Q_surface = (EC.enthalpy(270.0, 0.0, 0.0) - 25*Lz - E_base) / Lz
    E_exact = exact_DN(Lz, E_base, Q_surface)

    E_steady = E_base + Q_surface * z

    with PISM.vec.Access(nocomm=[column.enthalpy,
                                 column.u, column.v, column.w,
                                 column.strain_heating]):
        column.sys.fine_to_coarse(E_exact(z, 0), 1, 1, column.enthalpy)
        column.reset_flow()
        column.reset_strain_heating()

        t = 0.0
        while t < T_final:
            column.init_column()

            column.sys.set_surface_heat_flux(K * Q_surface)
            column.sys.set_basal_dirichlet(E_base)

            x = column.sys.solve()

            column.sys.fine_to_coarse(x, 1, 1, column.enthalpy)

            t += dt

    E_exact_final = E_exact(z, t)

    if plot_results:
        t_years = PISM.convert(unit_system, t, "seconds", "years")

        plt.figure()
        plt.xlabel("z, meters")
        plt.ylabel("E, J/kg")
        plt.hold(True)
        plt.step(z, E_exact(z, 0), color="blue", label="initial condition")
        plt.step(z, E_exact_final, color="green", label="exact solution")
        plt.step(z, cts(pressure(Lz - z)), "--", color="black", label="CTS")
        plt.step(z, E_steady, "--", color="green", label="steady state profile")
        plt.grid(True)

        plt.step(z, x, label="T={}".format(t_years), color="red")

        plt.legend(loc="best")
        plt.show()

    errors = E_exact(z, t) - x

    max_error = np.max(np.fabs(errors))
    avg_error = np.average(np.fabs(errors))

    return max_error, avg_error

def exact_ND(L, Q0, UL):
    """Exact solution (and an initial state) for the 'Dirichlet at the base,
    Neumann at the top' setup."""
    n = 1
    lambda_n = 1.0 / L * (-np.pi / 2.0 + n * np.pi)
    a = L * 25.0

    def f(z, t):
        v = a * np.exp(-lambda_n**2 * alpha2 * t) * np.sin(lambda_n * (L - z))
        return v + (UL + Q0 * (z - L))
    return f

def errors_ND(plot_results=True, T_final_years=1000.0, dt_years=100, Mz=101):
    """Test the enthalpy solver with Neumann B.C. at the base and
    Dirichlet B.C. at the top surface.
    """
    T_final = PISM.convert(unit_system, T_final_years, "years", "seconds")
    dt = PISM.convert(unit_system, dt_years, "years", "seconds")

    column = EnthalpyColumn(Mz, dt)

    Lz = column.Lz
    z = np.array(column.sys.z())

    E_surface = EC.enthalpy(260.0, 0.0, 0.0)
    E_base = EC.enthalpy(240.0, 0.0, EC.pressure(Lz))
    Q_base = (E_surface  - E_base) / Lz
    E_exact = exact_ND(Lz, Q_base, E_surface)

    E_steady = E_surface + Q_base * (z - Lz)

    with PISM.vec.Access(nocomm=[column.enthalpy,
                                 column.u, column.v, column.w,
                                 column.strain_heating]):
        column.sys.fine_to_coarse(E_exact(z, 0), 1, 1, column.enthalpy)
        column.reset_flow()
        column.reset_strain_heating()

        t = 0.0
        while t < T_final:
            column.init_column()

            column.sys.set_basal_heat_flux(K * Q_base)
            column.sys.set_surface_dirichlet(E_surface)

            x = column.sys.solve()

            column.sys.fine_to_coarse(x, 1, 1, column.enthalpy)

            t += dt

    E_exact_final = E_exact(z, t)

    if plot_results:
        t_years = PISM.convert(unit_system, t, "seconds", "years")

        plt.figure()
        plt.xlabel("z, meters")
        plt.ylabel("E, J/kg")
        plt.hold(True)
        plt.step(z, E_exact(z, 0), color="blue", label="initial condition")
        plt.step(z, E_exact_final, color="green", label="exact solution")
        plt.step(z, cts(pressure(Lz - z)), "--", color="black", label="CTS")
        plt.step(z, E_steady, "--", color="green", label="steady state profile")
        plt.grid(True)

        plt.step(z, x, label="T={}".format(t_years), color="red")

        plt.legend(loc="best")
        plt.show()

    errors = E_exact(z, t) - x

    max_error = np.max(np.fabs(errors))
    avg_error = np.average(np.fabs(errors))

    return max_error, avg_error

convergence_rate_time("Dirichlet at the base, Neumann at the surface", errors_DN)
convergence_rate_space("Dirichlet at the base, Neumann at the surface", errors_DN)

convergence_rate_time("Neumann at the base, Dirichlet at the surface", errors_ND)
convergence_rate_space("Neumann at the base, Dirichlet at the surface", errors_ND)
