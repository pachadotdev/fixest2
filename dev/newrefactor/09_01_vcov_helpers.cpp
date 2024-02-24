#include "09_0_vcov.hpp"

inline double to_sq(double x) { return x * x; }

inline double dist_km(double lon_1, double lat_1, double cos_lat_1,
                      double lon_2, double lat_2, double cos_lat_2) {
  double delta_lon = (lon_2 - lon_1) / 2;
  double delta_lat = (lat_2 - lat_1) / 2;

  double a =
      to_sq(sin(delta_lat)) + cos_lat_1 * cos_lat_2 * to_sq(sin(delta_lon));
  double res = 12752 * asin(fmin(1, sqrt(a)));

  return res;
}

inline double degree_to_radian(double x) { return x * 3.14159 / 180; }

inline double fabs_lon(double x, double y) {
  // there is a border problem that we take care of

  // this is in radians
  double diff = fabs(x - y);
  // in degrees it would be: diff < 180 ? diff : 360 - diff;
  return diff < 3.14159 ? diff : 6.28318 - diff;
}

inline double fabs_lat(double x, double y) {
  // There is no border problem wrt latitude
  return fabs(x - y);
}
