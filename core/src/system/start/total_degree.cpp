//This file is part of Bertini 2.
//
//total_degree.cpp is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//
//total_degree.cpp is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with total_degree.cpp.  If not, see <http://www.gnu.org/licenses/>.
//
// Copyright(C) 2015 - 2021 by Bertini2 Development Team
//
// See <http://www.gnu.org/licenses/> for a copy of the license, 
// as well as COPYING.  Bertini2 is provided with permitted 
// additional terms in the b2/licenses/ directory.

// individual authors of this file include:
// silviana amethyst, university of wisconsin eau claire

#include "bertini2/system/start/total_degree.hpp"


BOOST_CLASS_EXPORT(bertini::start_system::TotalDegree);


namespace bertini {
	using namespace bertini::node;
	
	namespace start_system {

		// constructor for TotalDegree start system, from any other *suitable* system.
		TotalDegree::TotalDegree(System const& s)
		{
			SanityChecks(s);
			CopyDegrees(s);
			CopyVariableStructure(s);
			SeedRandomValues(s.NumNaturalFunctions());
			GenerateFunctions();

			if (s.IsHomogeneous())
				Homogenize();

			if (s.IsPatched())
				CopyPatches(s);
		}// total degree constructor

		
		TotalDegree& TotalDegree::operator*=(Nd const& n)
		{
			*this *= n;
			return *this;
		}
		
		

		unsigned long long TotalDegree::NumStartPoints() const
		{
			unsigned long long num_start_points = 1;
			for (const auto& iter : degrees_)
				num_start_points*=iter;
			return num_start_points;
		}


		
		Vec<dbl> TotalDegree::GenerateStartPoint(dbl,unsigned long long index) const
		{
			Vec<dbl> start_point(NumVariables());
			auto indices = IndexToSubscript(index, degrees_);

			unsigned offset = 0;
			if (IsPatched())
			{
				start_point(0) = dbl(1);
				offset = 1;
			}

			auto two_i_pi = std::acos(-1.0) * dbl(0,2);

			for (size_t ii = 0; ii< NumNaturalVariables(); ++ii)
				start_point(ii+offset) = exp( two_i_pi * static_cast<double>(indices[ii]) / static_cast<double>(degrees_[ii])  ) * pow(random_values_[ii]->Eval<dbl>(), 1.0 / static_cast<double>(degrees_[ii]));

			if (IsPatched())
				RescalePointToFitPatchInPlace(start_point);

			return start_point;
		}


		Vec<mpfr_complex> TotalDegree::GenerateStartPoint(mpfr_complex,unsigned long long index) const
		{
			using bertini::DefaultPrecision;

			Vec<mpfr_complex> start_point(NumVariables()); // make the value we're returning
			auto indices = IndexToSubscript(index, degrees_); // get the position of it -- used in the angle of the coordinates of the produced point.

			unsigned offset = 0;
			if (IsPatched())
			{
				start_point(0) = mpfr_complex(1,0,DefaultPrecision());
				offset = 1;
			}

// TODO: this code should be cleaned up after issue 308 is solved -- namely, the two precision adjustment calls should be removed.  They're only necessary because prec16 / ulonglog = prec19.

			auto one = mpfr_float(1);
			mpfr_complex two_i_pi = mpfr_complex(0,2) * acos( mpfr_float(-1) );
			for (size_t ii = 0; ii< NumNaturalVariables(); ++ii)
			{
				mpfr_complex a = exp( (two_i_pi * indices[ii]) / degrees_[ii]);
				mpfr_complex b = pow(random_values_[ii]->Eval<mpfr_complex>(), one / degrees_[ii]);

				Precision(a,DefaultPrecision());
				Precision(b,DefaultPrecision());

				start_point(ii+offset) = a*b;
			}

			if (IsPatched())
				RescalePointToFitPatchInPlace(start_point);

			return start_point;
		}

		inline
		TotalDegree operator*(TotalDegree td, std::shared_ptr<node::Node> const& n)
		{
			td *= n;
			return td;
		}

		void TotalDegree::SanityChecks(System const& s)
		{
			if (s.NumHomVariableGroups() > 0)
				throw std::runtime_error("a homogeneous variable group is present.  currently unallowed");

			if (s.NumTotalFunctions() != s.NumVariables())
				throw std::runtime_error("attempting to construct total degree start system from non-square target system");

			if (s.HavePathVariable())
				throw std::runtime_error("attempting to construct total degree start system, but target system has path varible declared already");

			if (s.NumVariableGroups() != 1)
				throw std::runtime_error("more than one affine variable group.  currently unallowed");

			if (!s.IsPolynomial())
				throw std::runtime_error("attempting to construct total degree start system from non-polynomial target system");
		}

		void TotalDegree::CopyDegrees(System const& s)
		{
			auto deg = s.Degrees();
			for (const auto& d : deg)
				degrees_.push_back(static_cast<const size_t>(d));
		}


		void TotalDegree::SeedRandomValues(int num_functions)
		{
			random_values_.resize(num_functions);
			for (unsigned ii = 0; ii < num_functions; ++ii)
				random_values_[ii] = Rational::Make(node::Rational::Rand());
		}

		void TotalDegree::GenerateFunctions()
		{
			// by hypothesis, the system has a single variable group.
			auto v = this->AffineVariableGroup(0);
			for (auto iter = v.begin(); iter!=v.end(); iter++)
				AddFunction(pow(*iter,(int) *(degrees_.begin() + (iter-v.begin()))) - random_values_[iter-v.begin()]);
		}
	} // namespace start_system
} //namespace bertini
