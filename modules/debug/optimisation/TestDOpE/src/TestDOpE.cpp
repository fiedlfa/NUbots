/*
 * This file is part of NUbots Codebase.
 *
 * The NUbots Codebase is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The NUbots Codebase is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the NUbots Codebase.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2015 NUbots <nubots@nubots.net>
 */

#include "TestDOpE.h"

#include "messages/support/Configuration.h"
#include "messages/support/optimisation/DOpE.h"

namespace modules {
namespace debug {
namespace optimisation {

    using messages::support::Configuration;
    using messages::support::optimisation::RequestParameters;
    using messages::support::optimisation::RegisterOptimisation;

    TestDOpE::TestDOpE(std::unique_ptr<NUClear::Environment> environment)
    : Reactor(std::move(environment)) {

        on<Configuration>("TestDOpE.yaml").then([this] (const Configuration& config) {
            // Use configuration here from file TestDOpE.yaml
        });

        emit<Scope::INITIALIZE>(std::make_unique<RegisterOptimisation>(RegisterOptimisation {
            "test_dope",
            arma::vec({1,2,3,4,5}),
            arma::vec({1,2,3,4,5}),
            true
        }));
    }
}
}
}
