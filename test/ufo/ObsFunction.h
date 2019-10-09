/*
 * (C) Copyright 2019 UCAR
 * 
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0. 
 */

#ifndef TEST_UFO_OBSFUNCTION_H_
#define TEST_UFO_OBSFUNCTION_H_

#include <memory>
#include <string>
#include <vector>

#define ECKIT_TESTING_SELF_REGISTER_CASES 0

#include "eckit/config/LocalConfiguration.h"
#include "eckit/testing/Test.h"
#include "ioda/ObsSpace.h"
#include "ioda/ObsVector.h"
#include "oops/../test/TestEnvironment.h"
#include "oops/runs/Test.h"
#include "ufo/filters/ObsFilterData.h"
#include "ufo/filters/obsfunctions/ObsFunction.h"
#include "ufo/filters/Variables.h"
#include "ufo/GeoVaLs.h"
#include "ufo/ObsDiagnostics.h"

namespace ufo {
namespace test {

// -----------------------------------------------------------------------------

float dataVectorDiff(const ioda::ObsSpace & ospace, ioda::ObsDataVector<float> & vals,
                    const ioda::ObsDataVector<float> & ref) {
  float rms = 0.0;
  int nobs = 0;
  for (size_t jj = 0; jj < ref.nlocs() ; ++jj) {
    vals[0][jj] -= ref[0][jj];
    rms += vals[0][jj] * vals[0][jj];
    nobs++;
  }
  ospace.comm().allReduceInPlace(rms, eckit::mpi::sum());
  ospace.comm().allReduceInPlace(nobs, eckit::mpi::sum());
  if (nobs > 0) rms = sqrt(rms / static_cast<float>(nobs));
  return rms;
}

// -----------------------------------------------------------------------------

void testFunction() {
  const eckit::LocalConfiguration conf = ::test::TestEnvironment::config();
///  Setup ObsSpace
  util::DateTime bgn(conf.getString("window_begin"));
  util::DateTime end(conf.getString("window_end"));
  const eckit::LocalConfiguration obsconf(conf, "ObsSpace");
  ioda::ObsSpace ospace(obsconf, bgn, end);

///  Setup ObsFilterData
  ObsFilterData inputs(ospace);

///  Get function name and which group to use for H(x)
  const eckit::LocalConfiguration obsfuncconf(conf, "ObsFunction");
  std::string funcname = obsfuncconf.getString("name");

///  Setup function
  ObsFunction obsfunc(funcname);
  ufo::Variables allfuncvars = obsfunc.requiredVariables();

///  Setup GeoVaLs
  const oops::Variables geovars = allfuncvars.allFromGroup("GeoVaLs");
  std::unique_ptr<GeoVaLs> gval;
  if (geovars.size() > 0) {
    const eckit::LocalConfiguration gconf(conf, "GeoVaLs");
    gval.reset(new GeoVaLs(gconf, ospace, geovars));
    inputs.associate(*gval);
  }

///  Setup ObsDiags
  const oops::Variables diagvars = allfuncvars.allFromGroup("ObsDiag");
  std::unique_ptr<ObsDiagnostics> diags;
  if (diagvars.size() > 0) {
    const eckit::LocalConfiguration diagconf(conf, "ObsDiag");
    diags.reset(new ObsDiagnostics(diagconf, ospace, diagvars));
    inputs.associate(*diags);
  }

///  Get output variable names
  const oops::Variables outputvars(obsfuncconf);
///  Compute function result
  ioda::ObsDataVector<float> vals(ospace, outputvars, "ObsFunction", false);
  obsfunc.compute(inputs, vals);
  vals.save("TestResult");

///  Compute function result through ObsFilterData
  ioda::ObsDataVector<float> vals_ofd(ospace, outputvars, "ObsFunction", false);
  inputs.get(funcname+"@ObsFunction", vals_ofd);

///  Read reference values from ObsSpace
  ioda::ObsDataVector<float> ref(ospace, outputvars, "TestReference");

  const double tol = obsfuncconf.getDouble("tolerance");

///  Calculate rms(f(x) - ref) and compare to tolerance
  float rms = dataVectorDiff(ospace, vals, ref);
  oops::Log::info() << "Vector difference between reference and computed: " << vals << std::endl;
  EXPECT(rms < 100*tol);  //  change tol from percent to actual value.

  rms = dataVectorDiff(ospace, vals_ofd, ref);
  oops::Log::info() << "Vector difference between reference and computed via ObsFilterData: "
                    << vals_ofd << std::endl;
  EXPECT(rms < 100*tol);  //  change tol from percent to actual value.
}

// -----------------------------------------------------------------------------

class ObsFunction : public oops::Test {
 public:
  ObsFunction() {}
  virtual ~ObsFunction() {}
 private:
  std::string testid() const {return "ufo::test::ObsFunction";}

  void register_tests() const {
    std::vector<eckit::testing::Test>& ts = eckit::testing::specification();

    ts.emplace_back(CASE("ufo/ObsFunction/testFunction")
      { testFunction(); });
  }
};

// -----------------------------------------------------------------------------

}  // namespace test
}  // namespace ufo

#endif  // TEST_UFO_OBSFUNCTION_H_