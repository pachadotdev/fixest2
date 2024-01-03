// list of objects that will be used to
// lighten the writting of the functions
// Contains only parameters that are fixed throguhout ALL the process

#include "01_0_convergence.hpp"

void CCC_poisson(int n_obs, int nb_cluster, double *cluster_coef,
                 double *exp_mu, double *sum_y, int *dum) {
  // initialize cluster coef
  std::fill(cluster_coef, cluster_coef + nb_cluster, 0.0);

  // looping sequentially over exp_mu
  for (int i = 0; i < n_obs; ++i) {
    cluster_coef[dum[i]] += exp_mu[i];
  }

  // calculating cluster coef
  for (int m = 0; m < nb_cluster; ++m) {
    cluster_coef[m] = sum_y[m] / cluster_coef[m];
  }
}

void CCC_poisson_2(const std::vector<double> &pcluster_origin,
                   std::vector<double> &pcluster_destination, int n_i, int n_j,
                   int n_cells, const std::vector<int> &mat_row,
                   std::vector<int> &mat_col, std::vector<double> &mat_value,
                   const std::vector<double> &ca, const std::vector<double> &cb,
                   std::vector<double> &alpha) {
  double *beta = pcluster_destination.data() + n_i;

  std::fill(alpha.begin(), alpha.begin() + n_i, 0.0);
  std::fill(beta, beta + n_j, 0.0);

  for (int obs = 0; obs < n_cells; ++obs) {
    int row = mat_row[obs];
    int col = mat_col[obs];
    double value = mat_value[obs];
    beta[col] += value * pcluster_origin[row];
  }

  for (int j = 0; j < n_j; ++j) {
    beta[j] = cb[j] / beta[j];
  }

  for (int obs = 0; obs < n_cells; ++obs) {
    int row = mat_row[obs];
    int col = mat_col[obs];
    double value = mat_value[obs];
    alpha[row] += value * beta[col];
  }

  for (int i = 0; i < n_i; ++i) {
    pcluster_destination[i] = ca[i] / alpha[i];
  }
}

void CCC_poisson_log(int n_obs, int nb_cluster, double *cluster_coef,
                     double *mu, double *sum_y, int *dum) {
  // compute cluster coef, poisson
  // This is log poisson => we are there because classic poisson did not work
  // Thus high chance there are very high values of the cluster coefs (in abs
  // value) we need to take extra care in computing it => we apply trick of
  // substracting the max in the exp

  vector<double> mu_max(nb_cluster);
  vector<bool> doInit(nb_cluster, true);

  std::fill_n(cluster_coef, nb_cluster, 0.0);

  // finding the max mu for each cluster
  for (int i = 0; i < n_obs; ++i) {
    int d = dum[i];
    double mu_i = mu[i];
    double &mu_max_d = mu_max[d];

    if (doInit[d]) {
      mu_max_d = mu_i;
      doInit[d] = false;
    } else if (mu_i > mu_max_d) {
      mu_max_d = mu_i;
    }
  }

  // looping sequentially over exp_mu
  for (int i = 0; i < n_obs; ++i) {
    int d = dum[i];
    double mu_i = mu[i];
    double mu_max_d = mu_max[d];
    cluster_coef[d] += exp(mu_i - mu_max_d);
  }

  // calculating cluster coef
  for (int m = 0; m < nb_cluster; ++m) {
    double sum_y_m = sum_y[m];
    double &cluster_coef_m = cluster_coef[m];
    double mu_max_m = mu_max[m];

    cluster_coef_m = log(sum_y_m) - log(cluster_coef_m) - mu_max_m;
  }
}

// GAUSSIAN

void CCC_gaussian(int n_obs, int nb_cluster, double *cluster_coef, double *mu,
                  double *sum_y, int *dum, int *table) {
  // compute cluster coef, gaussian

  // initialize cluster coef
  std::fill_n(cluster_coef, nb_cluster, 0.0);

  // looping sequentially over mu
  for (int i = 0; i < n_obs; ++i) {
    int dum_i = dum[i];
    double mu_i = mu[i];
    cluster_coef[dum_i] += mu_i;
  }

  // calculating cluster coef
  for (int m = 0; m < nb_cluster; ++m) {
    double sum_y_m = sum_y[m];
    double cluster_coef_m = cluster_coef[m];
    int table_m = table[m];

    cluster_coef[m] = (sum_y_m - cluster_coef_m) / table_m;
  }
}

void CCC_gaussian_2(const vector<double> &pcluster_origin,
                    vector<double> &pcluster_destination, int n_i, int n_j,
                    int n_cells, int *mat_row, int *mat_col,
                    double *mat_value_Ab, double *mat_value_Ba,
                    const vector<double> &a_tilde, vector<double> &beta) {
  // Initialize pcluster_destination and beta
  std::copy(a_tilde.begin(), a_tilde.begin() + n_i,
            pcluster_destination.begin());
  std::fill(beta.begin(), beta.begin() + n_j, 0);

  // Compute beta and update pcluster_destination
  for (int obs = 0; obs < n_cells; ++obs) {
    int col = mat_col[obs];
    int row = mat_row[obs];
    beta[col] += mat_value_Ba[obs] * pcluster_origin[row];
    pcluster_destination[row] += mat_value_Ab[obs] * beta[col];
  }
}

// NEGATIVE BINOMIAL and LOGIT (helper + negbin + logit)

double computeClusterCoefficient(int m, double theta, double *mu, double *lhs,
                                 double *sum_y, int *obsCluster, int *cumtable,
                                 double lower_bound, double upper_bound,
                                 double diffMax_NR, int iterFullDicho,
                                 int iterMax) {
  // we loop over each cluster
  // we initialise the cluster coefficient at 0 (it should converge to 0 at
  // some point)
  double x1 = 0;
  bool keepGoing = true;
  int iter = 0, i;
  int u0 = (m == 0 ? 0 : cumtable[m - 1]);

  double value, x0, derivative = 0, exp_mu;

  // Update of the value if it goes out of the boundaries
  // because we dont know ex ante if 0 is within the bounds
  if (x1 >= upper_bound || x1 <= lower_bound) {
    x1 = (lower_bound + upper_bound) / 2;
  }

  while (keepGoing) {
    ++iter;

    // 1st step: start the bounds

    // computing the value of f(x)
    value = sum_y[m];
    for (int u = u0; u < cumtable[m]; ++u) {
      int i = obsCluster[u];
      double denominator = 1 + theta * exp(-x1 - mu[i]);
      value -= (lhs ? theta + lhs[i] : theta) / denominator;
    }

    // update bounds
    if (value > 0) {
      lower_bound = x1;
    } else {
      upper_bound = x1;
    }

    // 2nd step: NR iteration or Dichotomy

    x0 = x1;
    if (value == 0) {
      keepGoing = false;
    } else if (iter <= iterFullDicho) {
      // computing the derivative
      derivative = 0;
      for (int u = u0; u < cumtable[m]; ++u) {
        i = obsCluster[u];
        exp_mu = exp(x1 + mu[i]);
        derivative -= theta * (theta + lhs[i]) /
                      ((theta / exp_mu + 1) * (theta + exp_mu));

        double numerator = (lhs ? theta * (theta + lhs[i]) : theta * theta);
        derivative -= numerator * exp_mu / (1 + theta * exp_mu);
      }

      x1 = x0 - value / derivative;

      // 3rd step: dichotomy (if necessary)
      // Update of the value if it goes out of the boundaries
      if (x1 >= upper_bound || x1 <= lower_bound) {
        x1 = (lower_bound + upper_bound) / 2;
      }
    } else {
      x1 = (lower_bound + upper_bound) / 2;
    }

    // the stopping criterion
    if (iter == iterMax) {
      keepGoing = false;
      if (omp_get_thread_num() == 0) {
        Rprintf(
            "[Getting cluster coefficients nber %i] max iterations reached "
            "(%i).\n",
            m, iterMax);
        Rprintf("Value Sum Deriv (NR) = %f. Difference = %f.\n", value,
                fabs(x0 - x1));
      }
    }

    if (stopping_criterion(x0, x1, diffMax_NR)) {
      keepGoing = false;
    }
  }

  return x1;
}

void CCC_negbin(int nthreads, int nb_cluster, double theta, double diffMax_NR,
                double *cluster_coef, double *mu, double *lhs, double *sum_y,
                int *obsCluster, int *table, int *cumtable) {
  // first we find the min max for each cluster to get the bounds
  int iterMax = 100, iterFullDicho = 10;

  // finding the max/min values of mu for each cluster
  vector<double> lower_bound(nb_cluster);
  vector<double> upper_bound(nb_cluster);

  // attention lower_bound => when mu is maximum
  int u0;
  double value, mu_min, mu_max;

  for (int m = 0; m < nb_cluster; ++m) {
    // the min/max of mu
    u0 = (m == 0 ? 0 : cumtable[m - 1]);
    mu_min = mu[obsCluster[u0]];
    mu_max = mu[obsCluster[u0]];
    for (int u = 1 + u0; u < cumtable[m]; ++u) {
      value = mu[obsCluster[u]];
      if (value < mu_min) {
        mu_min = value;
      } else if (value > mu_max) {
        mu_max = value;
      }
    }

    // computing the bounds
    lower_bound[m] =
        log(sum_y[m]) - log(static_cast<double>(table[m])) - mu_max;
    upper_bound[m] = lower_bound[m] + (mu_max - mu_min);
  }

#pragma omp parallel for num_threads(nthreads)
  for (int m = 0; m < nb_cluster; ++m) {
    // after convergence: update cluster coef
    cluster_coef[m] = computeClusterCoefficient(
        m, theta, mu, lhs, sum_y, obsCluster, cumtable, lower_bound[m],
        upper_bound[m], diffMax_NR, iterFullDicho, iterMax);
  }
}

void CCC_logit(int nthreads, int nb_cluster, double diffMax_NR,
               double *cluster_coef, double *mu, double *sum_y, int *obsCluster,
               int *table, int *cumtable) {
  // compute cluster coefficients negbin
  // This is not direct: needs to apply dichotomy+NR algorithm

  // first we find the min max for each cluster to get the bounds
  int iterMax = 100, iterFullDicho = 10;

  // finding the max/min values of mu for each cluster
  vector<double> lower_bound(nb_cluster);
  vector<double> upper_bound(nb_cluster);

  // attention lower_bound => when mu is maximum
  int u0;
  double value, mu_min, mu_max;

  for (int m = 0; m < nb_cluster; ++m) {
    // the min/max of mu
    u0 = (m == 0 ? 0 : cumtable[m - 1]);
    mu_min = mu[obsCluster[u0]];
    mu_max = mu[obsCluster[u0]];
    for (int u = 1 + u0; u < cumtable[m]; ++u) {
      value = mu[obsCluster[u]];
      if (value < mu_min) {
        mu_min = value;
      } else if (value > mu_max) {
        mu_max = value;
      }
    }

    // computing the "bornes"
    lower_bound[m] = log(sum_y[m]) - log(table[m] - sum_y[m]) - mu_max;
    upper_bound[m] = lower_bound[m] + (mu_max - mu_min);
  }

  //
  // Parallel loop
  //

#pragma omp parallel for num_threads(nthreads)
  for (int m = 0; m < nb_cluster; ++m) {
    // after convergence: update cluster coef
    cluster_coef[m] = computeClusterCoefficient(
        m, 0, mu, NULL, sum_y, obsCluster, cumtable, lower_bound[m],
        upper_bound[m], diffMax_NR, iterFullDicho, iterMax);
  }
}
