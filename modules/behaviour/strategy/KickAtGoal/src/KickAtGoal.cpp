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
 * Copyright 2013 NUBots <nubots@nubots.net>
 */

#include "KickAtGoal.h"
#include <armadillo>

#include "messages/behaviour/KickPlan.h"
#include "messages/behaviour/WalkStrategy.h"
#include "messages/support/Configuration.h"
#include "messages/vision/VisionObjects.h"
#include "utility/time/time.h"

namespace modules {
namespace behaviour {
namespace strategy {

    using messages::behaviour::WalkApproach;
    using messages::behaviour::WalkTarget;
    using messages::behaviour::WalkStrategy;
    using messages::behaviour::KickPlan;
    using messages::support::Configuration;
    using messages::behaviour::proto::Behaviour;
    using VisionBall = messages::vision::Ball;
    using VisionGoal = messages::vision::Goal;
    using utility::time::durationFromSeconds;

    KickAtGoal::KickAtGoal(std::unique_ptr<NUClear::Environment> environment) : Reactor(std::move(environment)) {

        // TODO: unhack?
        emit(std::make_unique<KickPlan>(KickPlan{{3, 0}}));

        on<Trigger<Every<30, Per<std::chrono::seconds>>>, Options<Single>>([this](const time_t&) {

            doBehaviour();

        });

        on<Trigger<std::vector<VisionBall>>>([this] (const std::vector<VisionBall>& balls) {
            if (!balls.empty()) {
                ballLastSeen = NUClear::clock::now();
            }
        });

        on<Trigger<std::vector<VisionGoal>>>([this] (const std::vector<VisionGoal>& goals) {
            if (!goals.empty()) {
                goalLastSeen = NUClear::clock::now();
            }
        });

        on<Trigger<Configuration<KickAtGoal>>>([this](const Configuration<KickAtGoal>& config) {

            ballActiveTimeout = durationFromSeconds(config["ball_active_timeout"].as<double>());

        });

    }

    void KickAtGoal::doBehaviour() {
        // Store the state before executing behaviour.
        Behaviour::State previousState = currentState;

        // Check if the ball  has been seen recently.
        if (NUClear::clock::now() - ballLastSeen < ballActiveTimeout) {
            walkToBall();
        } 
        else {
            spinToWin();
        }

        if (currentState != previousState) {
            emit(std::make_unique<Behaviour::State>(currentState));
        }

    }

    void KickAtGoal::walkToBall() {
        auto approach = std::make_unique<WalkStrategy>();
        approach->targetPositionType = WalkTarget::Ball;
        approach->targetHeadingType = WalkTarget::WayPoint;
        approach->walkMovementType = WalkApproach::WalkToPoint;
        approach->heading = arma::vec2({3, 0}); // TODO: unhack
        emit(std::move(approach));
        currentState = Behaviour::WALK_TO_BALL;
    }

    void KickAtGoal::spinToWin() {
        // TODO: does this work?
        auto command = std::make_unique<WalkStrategy>();
        command->walkMovementType = WalkApproach::DirectCommand;
        command->target = {0,0};
        command->heading = {1,0};
        emit(std::move(command));
        currentState = Behaviour::SEARCH_FOR_BALL;
    }

}
}
}

