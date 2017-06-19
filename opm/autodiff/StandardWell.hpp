/*
  Copyright 2017 SINTEF ICT, Applied Mathematics.
  Copyright 2017 Statoil ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef OPM_STANDARDWELL_HEADER_INCLUDED
#define OPM_STANDARDWELL_HEADER_INCLUDED


#include "config.h"

#include <opm/autodiff/WellInterface.hpp>

#include<dune/common/fmatrix.hh>
#include<dune/istl/bcrsmatrix.hh>
#include<dune/istl/matrixmatrix.hh>

#include <opm/material/densead/Math.hpp>
#include <opm/material/densead/Evaluation.hpp>

namespace Opm
{

    template<typename TypeTag>
    class StandardWell: public WellInterface<TypeTag>
    {

    public:
        // using WellInterface<TypeTag>::Simulator;
        // using WellInterface<TypeTag>::WellState;
        typedef typename WellInterface<TypeTag>::Simulator Simulator;
        typedef typename WellInterface<TypeTag>::WellState WellState;
        // the positions of the primary variables for StandardWell
        // there are three primary variables, the second and the third ones are F_w and F_g
        // the first one can be total rate (G_t) or bhp, based on the control
        enum WellVariablePositions {
            XvarWell = 0,
            WFrac = 1,
            GFrac = 2
        };


        typedef double Scalar;
        // static const int numEq = BlackoilIndices::numEq;
        static const int numEq = 3;
        static const int numWellEq = numEq; //number of wellEq is the same as numEq in the model
        static const int solventCompIdx = 3; //TODO get this from ebos
        typedef Dune::FieldVector<Scalar, numEq    > VectorBlockType;
        typedef Dune::FieldMatrix<Scalar, numEq, numEq > MatrixBlockType;
        typedef Dune::BCRSMatrix <MatrixBlockType> Mat;
        typedef Dune::BlockVector<VectorBlockType> BVector;
        typedef DenseAd::Evaluation<double, /*size=*/numEq + numWellEq> EvalWell;
        typedef DenseAd::Evaluation<double, /*size=*/numEq> Eval;

        // for now, using the matrix and block version in StandardWellsDense.
        // TODO: for bettern generality, it should contain blocksize_field and blocksize_well.
        // They are allowed to be different and it will create four types of matrix blocks and two types of
        // vector blocks.

        /* const static int blocksize = 3;
        typedef double Scalar;
        typedef Dune::FieldVector<Scalar, blocksize    > VectorBlockType;
        typedef Dune::FieldMatrix<Scalar, blocksize, blocksize > MatrixBlockType;
        typedef Dune::BCRSMatrix <MatrixBlockType> Mat;
        typedef Dune::BlockVector<VectorBlockType> BVector;
        typedef DenseAd::Evaluation<double, blocksize + blocksize> EvalWell; */
        /* using WellInterface::EvalWell;
        using WellInterface::BVector;
        using WellInterface::Mat;
        using WellInterface::MatrixBlockType;
        using WellInterface::VectorBlockType; */

        StandardWell(const Well* well, const int time_step, const Wells* wells);

        /// the densities of the fluid  in each perforation
        virtual const std::vector<double>& perfDensities() const;
        virtual std::vector<double>& perfDensities();

        /// the pressure difference between different perforations
        virtual const std::vector<double>& perfPressureDiffs() const;
        virtual std::vector<double>& perfPressureDiffs();

        virtual void assembleWellEq(Simulator& ebos_simulator,
                                    const double dt,
                                    WellState& well_state,
                                    bool only_wells);

        virtual void setWellVariables(const WellState& well_state);

        EvalWell wellVolumeFractionScaled(const int phase) const;

        EvalWell wellVolumeFraction(const int phase) const;

        EvalWell wellSurfaceVolumeFraction(const int phase) const;

        using WellInterface<TypeTag>::phaseUsage;
        using WellInterface<TypeTag>::active;
        using WellInterface<TypeTag>::numberOfPerforations;
        using WellInterface<TypeTag>::indexOfWell;
        using WellInterface<TypeTag>::name;
        using WellInterface<TypeTag>::wellType;
        using WellInterface<TypeTag>::wellControls;
        using WellInterface<TypeTag>::compFrac;
        using WellInterface<TypeTag>::numberOfPhases;
        using WellInterface<TypeTag>::perfDepth;

    protected:

        using WellInterface<TypeTag>::vfp_properties_;
        using WellInterface<TypeTag>::gravity_;

        // densities of the fluid in each perforation
        std::vector<double> perf_densities_;
        // pressure drop between different perforations
        std::vector<double> perf_pressure_diffs_;

        // TODO: probably, they should be moved to the WellInterface, when
        // we decide the template paramters.
        // two off-diagonal matrices
        Mat dune_B_;
        Mat dune_C_;
        // diagonal matrix for the well
        Mat inv_dune_D_;

        BVector res_well_;

        std::vector<EvalWell> well_variables_;

        // TODO: this function should be moved to the base class.
        // while it faces chanllenges for MSWell later, since the calculation of bhp
        // based on THP is never implemented for MSWell yet.
        EvalWell getBhp() const;

        // TODO: it is also possible to be moved to the base class.
        EvalWell getQs(const int phase) const;
    };

}

#include "StandardWell_impl.hpp"

#endif // OPM_STANDARDWELL_HEADER_INCLUDED
