#include "MPC.h"
#include <cppad/cppad.hpp>
#include <cppad/ipopt/solve.hpp>
#include "Eigen-3.3/Eigen/Core"

using CppAD::AD;

// TODO: Set the timestep length and duration
size_t N = 12;
double dt = 0.05;
int latency_ind = 2; //in units of dt (100ms)
// This value assumes the model presented in the classroom is used.
//
// It was obtained by measuring the radius formed by running the vehicle in the
// simulator around in a circle with a constant steering angle and velocity on a
// flat terrain.
//
// Lf was tuned until the the radius formed by the simulating the model
// presented in the classroom matched the previous radius.
//
// This is the length from front to CoG that has a similar radius.
const double Lf = 2.67;

double ref_v = 75;
double ref_cte = 0;
double ref_epsi = 0;

// Var start indices for different variable types:
size_t x_start_ind = 0;
size_t y_start_ind = x_start_ind + N;
size_t psi_start_ind = y_start_ind + N;
size_t v_start_ind = psi_start_ind + N;
size_t cte_start_ind = v_start_ind + N;
size_t epsi_start_ind = cte_start_ind + N;
size_t delta_start_ind = epsi_start_ind + N;
size_t a_start_ind = delta_start_ind + N -1;

class FG_eval {
 public:
  // Fitted polynomial coefficients
  Eigen::VectorXd coeffs;
  vector<double> previous_actutations;
  FG_eval(Eigen::VectorXd coeffs, vector<double>previous_actutations) { 
    this->coeffs = coeffs;
    this->previous_actutations = previous_actutations;
  }

  typedef CPPAD_TESTVECTOR(AD<double>) ADvector;
  void operator()(ADvector& fg, const ADvector& vars) {
    // TODO: implement MPC
    // `fg` a vector of the cost constraints, `vars` is a vector of variable values (state & actuators)
    // NOTE: You'll probably go back and forth between this function and
    // the Solver function below.
    fg[0] = 0;
    // The part of the cost based on the reference state.
    for (int t = 0; t < N; t++) {
      fg[0] += CppAD::pow(vars[cte_start_ind + t] - ref_cte, 2);
      fg[0] += CppAD::pow(vars[epsi_start_ind + t] - ref_epsi, 2);
      fg[0] += CppAD::pow(vars[v_start_ind + t] - ref_v, 2);
    }

    // Minimize the use of actuators.
    for (int t = 0; t < N - 1; t++) {
      fg[0] += CppAD::pow(vars[delta_start_ind + t], 2);
      fg[0] += CppAD::pow(vars[a_start_ind + t], 2);
    }

    // Minimize the value gap between sequential actuations.
    for (int t = 0; t < N - 2; t++) {
      fg[0] += 700 * CppAD::pow(vars[delta_start_ind + t + 1] - vars[delta_start_ind + t], 2);
      fg[0] += CppAD::pow(vars[a_start_ind + t + 1] - vars[a_start_ind + t], 2);
    }

    // Constraints
    fg[1 + x_start_ind] = vars[x_start_ind];
    fg[1 + y_start_ind] = vars[y_start_ind];
    fg[1 + psi_start_ind] = vars[psi_start_ind];
    fg[1 + v_start_ind] = vars[v_start_ind];
    fg[1 + cte_start_ind] = vars[cte_start_ind];
    fg[1 + epsi_start_ind] = vars[epsi_start_ind];

    // The rest of the constraints
    for (int i = 0; i < N - 1; i++) {
      // The state at time t+1 .
      AD<double> x1 = vars[x_start_ind + i + 1];
      AD<double> y1 = vars[y_start_ind + i + 1];
      AD<double> psi1 = vars[psi_start_ind + i + 1];
      AD<double> v1 = vars[v_start_ind + i + 1];
      AD<double> cte1 = vars[cte_start_ind + i + 1];
      AD<double> epsi1 = vars[epsi_start_ind + i + 1];

      // The state at time t.
      AD<double> x0 = vars[x_start_ind + i];
      AD<double> y0 = vars[y_start_ind + i];
      AD<double> psi0 = vars[psi_start_ind + i];
      AD<double> v0 = vars[v_start_ind + i];
      AD<double> cte0 = vars[cte_start_ind + i];
      AD<double> epsi0 = vars[epsi_start_ind + i];

      // Only consider the actuation at time t.
      AD<double> delta0 = vars[delta_start_ind + i];
      AD<double> a0 = vars[a_start_ind + i];

      AD<double> f0 = coeffs[0] + coeffs[1] * x0 + coeffs[2]*x0*x0 + coeffs[3]*x0*x0*x0;
      AD<double> psides0 = CppAD::atan(coeffs[1]+2*coeffs[2]*x0 + 3 * coeffs[3]*x0*x0);


      // Recall the equations for the model:
      // x_[t+1] = x[t] + v[t] * cos(psi[t]) * dt
      // y_[t+1] = y[t] + v[t] * sin(psi[t]) * dt
      // psi_[t+1] = psi[t] + v[t] / Lf * delta[t] * dt
      // v_[t+1] = v[t] + a[t] * dt
      // cte[t+1] = f(x[t]) - y[t] + v[t] * sin(epsi[t]) * dt
      // epsi[t+1] = psi[t] - psides[t] + v[t] * delta[t] / Lf * dt
      fg[2 + x_start_ind + i] = x1 - (x0 + v0 * CppAD::cos(psi0) * dt);
      fg[2 + y_start_ind + i] = y1 - (y0 + v0 * CppAD::sin(psi0) * dt);
      fg[2 + psi_start_ind + i] = psi1 - (psi0 + v0 * delta0 / Lf * dt);
      fg[2 + v_start_ind + i] = v1 - (v0 + a0 * dt);
      fg[2 + cte_start_ind + i] =
          cte1 - ((f0 - y0) + (v0 * CppAD::sin(epsi0) * dt));
      fg[2 + epsi_start_ind + i] =
          epsi1 - ((psi0 - psides0) + v0 * delta0 / Lf * dt);
    }
  }
};

//
// MPC class definition implementation.
//
MPC::MPC() {}
MPC::~MPC() {}

Solution MPC::Solve(Eigen::VectorXd state, Eigen::VectorXd coeffs) {
  bool ok = true;
  size_t i;
  typedef CPPAD_TESTVECTOR(double) Dvector;

  // TODO: Set the number of model variables (includes both states and inputs).
  // For example: If the state is a 4 element vector, the actuators is a 2
  // element vector and there are 10 timesteps. The number of variables is:
  //
  // 4 * 10 + 2 * 9
  double x = state[0];
  double y = state[1];
  double psi = state[2];
  double v = state[3];
  double cte = state[4];
  double epsi = state[5];

  size_t n_vars = N * 6 + (N - 1) * 2;
  // TODO: Set the number of constraints
  size_t n_constraints = N * 6;

  // Initial value of the independent variables.
  // SHOULD BE 0 besides initial state.
  Dvector vars(n_vars);
  for (int i = 0; i < n_vars; i++) {
    vars[i] = 0;
  }

  vars[x_start_ind] = x;
  vars[y_start_ind] = y;
  vars[psi_start_ind] = psi;
  vars[v_start_ind] = v;
  vars[cte_start_ind] = cte;
  vars[epsi_start_ind] = epsi;

  Dvector vars_lowerbound(n_vars);
  Dvector vars_upperbound(n_vars);
  // TODO: Set lower and upper limits for variables.

  // Set all non-actuators upper and lowerlimits
  // to the max negative and positive values.
  for (int i = 0; i < delta_start_ind; i++) {
    vars_lowerbound[i] = -1.0e17;
    vars_upperbound[i] = 1.0e17;
  }

  // The upper and lower limits of delta are set to -25 and 25 degrees (values in radians).
  for (int i = delta_start_ind; i < a_start_ind; i++) {
    vars_lowerbound[i] = -0.436332;
    vars_upperbound[i] = 0.436332;
  }

  // constrain delta to be the previous control for the latency time
  for (int i = delta_start_ind; i < delta_start_ind + latency_ind; i++) {
    vars_lowerbound[i] = delta_prev;
    vars_upperbound[i] = delta_prev;
  }

  // Acceleration/decceleration upper and lower limits.
  for (int i = a_start_ind; i < n_vars; i++) {
    vars_lowerbound[i] = -1.0;
    vars_upperbound[i] =  1.0;
  }

  // constrain a to be the previous control for the latency time 
  for (int i = a_start_ind; i < a_start_ind+latency_ind; i++) {
    vars_lowerbound[i] = a_prev;
    vars_upperbound[i] = a_prev;
  }

  // Lower and upper limits for the constraints
  // Should be 0 besides initial state.
  Dvector constraints_lowerbound(n_constraints);
  Dvector constraints_upperbound(n_constraints);
  for (int i = 0; i < n_constraints; i++) {
    constraints_lowerbound[i] = 0;
    constraints_upperbound[i] = 0;
  }

  constraints_lowerbound[x_start_ind] = x;
  constraints_lowerbound[y_start_ind] = y;
  constraints_lowerbound[psi_start_ind] = psi;
  constraints_lowerbound[v_start_ind] = v;
  constraints_lowerbound[cte_start_ind] = cte;
  constraints_lowerbound[epsi_start_ind] = epsi;

  constraints_upperbound[x_start_ind] = x;
  constraints_upperbound[y_start_ind] = y;
  constraints_upperbound[psi_start_ind] = psi;
  constraints_upperbound[v_start_ind] = v;
  constraints_upperbound[cte_start_ind] = cte;
  constraints_upperbound[epsi_start_ind] = epsi;

  // Object that computes objective and constraints
  vector<double> previous_actuations = {delta_prev,a_prev};

  // object that computes objective and constraints
  FG_eval fg_eval(coeffs, previous_actuations);

  //
  // NOTE: You don't have to worry about these options
  //
  // options for IPOPT solver
  std::string options;
  // Uncomment this if you'd like more print information
  options += "Integer print_level  0\n";
  // NOTE: Setting sparse to true allows the solver to take advantage
  // of sparse routines, this makes the computation MUCH FASTER. If you
  // can uncomment 1 of these and see if it makes a difference or not but
  // if you uncomment both the computation time should go up in orders of
  // magnitude.
  options += "Sparse  true        forward\n";
  options += "Sparse  true        reverse\n";
  // NOTE: Currently the solver has a maximum time limit of 0.5 seconds.
  // Change this as you see fit.
  options += "Numeric max_cpu_time          0.5\n";

  // place to return solution
  CppAD::ipopt::solve_result<Dvector> solution;

  // solve the problem
  CppAD::ipopt::solve<Dvector, FG_eval>(
      options, vars, vars_lowerbound, vars_upperbound, constraints_lowerbound,
      constraints_upperbound, fg_eval, solution);

  // Check some of the solution values
  ok &= solution.status == CppAD::ipopt::solve_result<Dvector>::success;
  

  Solution sol;
  for (auto i = 0; i < N-1 ; i++){
    //cout << i << ": " << "solution.x[x_start+i]: " << solution.x[x_start+i] << "solution.x[y_start+i]: " << solution.x[y_start+i] << endl;
    sol.X.push_back(solution.x[x_start_ind+i]);
    sol.Y.push_back(solution.x[y_start_ind+i]);
    sol.Delta.push_back(solution.x[delta_start_ind+i]);
    sol.A.push_back(solution.x[a_start_ind+i]);
  }

  // Cost
  auto cost = solution.obj_value;
  std::cout << "Cost " << cost << std::endl;

  // TODO: Return the first actuator values. The variables can be accessed with
  // `solution.x[i]`.
  //
  // {...} is shorthand for creating a vector, so auto x1 = {1.0,2.0}
  // creates a 2 element double vector.
  return sol;
}
