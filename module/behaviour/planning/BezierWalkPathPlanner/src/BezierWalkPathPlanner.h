/*
 * This file is part of the NUbots Codebase.
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

#ifndef MODULES_BEHAVIOUR_PLANNERS_BEZIERWALKPATHPLANNER_H
#define MODULES_BEHAVIOUR_PLANNERS_BEZIERWALKPATHPLANNER_H

#include <nuclear>
#include <armadillo>
#include <cmath>
#include "message/support/Configuration.h"
#include "message/input/Sensors.h"
#include "message/localisation/FieldObject.h"
#include "message/vision/VisionObjects.h"
#include "message/behaviour/MotionCommand.h"
 #include "message/behaviour/KickPlan.h"


namespace module {
    namespace behaviour {
        namespace planning {

                //using namespace message;
                /**
                 * Executes a getup script if the robot falls over.
                 *
                 * @author Josiah Walker
                 */
                class BezierWalkPathPlanner : public NUClear::Reactor {
                private:

                    /// @brief Subsumption ID key to access motors
                    const size_t subsumptionId;

                    //Thresholds
                    float turnSpeed;
                    float forwardSpeed;
                    float a;
                    float b;
                    float VP;
                    float VS;
                    float d1;
                    float d2;
                    float ErMax;

                    //-----------non-config variables (not defined in WalkPathPlanner.yaml)-----------

                    //info for the current walk
                    message::behaviour::MotionCommand latestCommand;
                    // arma::vec2 currentTargetPosition;
                    // arma::vec2 currentTargetHeading;
                    // message::behaviour::MotionCommand::Type planType;
                    // message::behaviour::KickPlan targetHeading;
                    // arma::vec2 targetPosition = {0, 0}; 

                public:
                    explicit BezierWalkPathPlanner(std::unique_ptr<NUClear::Environment> environment);
            };

        }  // planning
    }  // behaviours
}  // modules

#endif  // MODULES_BEHAVIOUR_PLANNERS_BEZIERWALKPATHPLANNER_H

