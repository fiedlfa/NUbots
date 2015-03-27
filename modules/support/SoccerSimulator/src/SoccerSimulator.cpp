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

#include "SoccerSimulator.h"
#include <nuclear>
#include "utility/math/angle.h"
#include "utility/math/coordinates.h"
#include "utility/nubugger/NUhelpers.h"
#include "utility/localisation/transform.h"
#include "utility/motion/ForwardKinematics.h"
#include "messages/vision/VisionObjects.h"
#include "messages/support/Configuration.h"
#include "messages/localisation/FieldObject.h"
#include "messages/input/Sensors.h"
#include "messages/input/ServoID.h"
#include "messages/platform/darwin/DarwinSensors.h"

namespace modules {
namespace support {

    using messages::input::Sensors;
    using messages::input::ServoID;
    using utility::math::angle::normalizeAngle;
    using utility::math::angle::vectorToBearing;
    using utility::math::angle::bearingToUnitVector;
    using utility::math::coordinates::cartesianToSpherical;
    using utility::motion::kinematics::calculateRobotToIMU;
    using utility::localisation::transform::SphericalRobotObservation;
    using utility::localisation::transform::WorldToRobotTransform;
    using utility::localisation::transform::RobotToWorldTransform;
    using utility::nubugger::graph;
    using messages::support::Configuration;
    using messages::support::FieldDescription;
    using messages::localisation::Mock;
    using messages::platform::darwin::DarwinSensors::Gyroscope;

    

    double triangle_wave(double t, double period) {
        auto a = period; // / 2.0;
        auto k = t / a;
        return 2.0 * std::abs(2.0 * (k - std::floor(k + 0.5))) - 1.0;
    }
    double sawtooth_wave(double t, double period) {
        return 2.0 * std::fmod(t / period, 1.0) - 1.0;
    }
    double square_wave(double t, double period) {
        return std::copysign(1.0, sawtooth_wave(t, period));
    }
    double sine_wave(double t, double period) {
        return std::sin((2.0 * M_PI * t) / period);
    }
    double absolute_time() {
        auto now = NUClear::clock::now();
        auto ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        double ms = static_cast<double>(ms_since_epoch - 1393322147502L);
        double t = ms / 1000.0;
        return t;
    }

    void SoccerSimulator::UpdateConfiguration(
        const messages::support::Configuration<SoccerSimulatorConfig>& config) {

        cfg_.simulate_goal_observations = config["vision"]["goal_observations"].as<bool>();
        cfg_.simulate_ball_observations = config["vision"]["ball_observations"].as<bool>();
        cfg_.observe_left_goal = config["vision"]["observe"]["left_goal"].as<bool>();
        cfg_.observe_right_goal = config["vision"]["observe"]["right_goal"].as<bool>();
        cfg_.distinguish_left_and_right_goals = config["vision"]["distinguish_left_and_right_goals"].as<bool>();

        cfg_.robot.motion_type = motionTypeFromString(config["robot"]["motion_type"].as<std::string>());
        cfg_.robot.path.period = config["robot"]["path"]["period"].as<float>();
        cfg_.robot.path.period = config["robot"]["path"]["x_amp"].as<float>();
        cfg_.robot.path.period = config["robot"]["path"]["y_amp"].as<float>();
        cfg_.robot.path.type = pathTypeFromString(config["robot"]["path"]["type"].as<std::string>());
      
        cfg_.ball.motion_type = motionTypeFromString(config["ball"]["motion_type"].as<std::string>());
        cfg_.ball.path.period = config["ball"]["path"]["period"].as<float>();
        cfg_.ball.path.period = config["ball"]["path"]["x_amp"].as<float>();
        cfg_.ball.path.period = config["ball"]["path"]["y_amp"].as<float>();
        cfg_.ball.path.type = pathTypeFromString(config["ball"]["path"]["type"].as<std::string>());
        
        cfg_.emit_robot_fieldobjects = config["nusight"]["emit_self"].as<bool>();
        cfg_.emit_ball_fieldobjects = config["nusight"]["emit_ball"].as<bool>();

    }

    SoccerSimulator::SoccerSimulator(std::unique_ptr<NUClear::Environment> environment)
        : Reactor(std::move(environment)) {

        on<Trigger<FieldDescription>>("FieldDescription Update", [this](const FieldDescription& desc) {
            field_description_ = std::make_shared<FieldDescription>(desc);
        });

        on<Trigger<Configuration<SoccerSimulatorConfig>>>(
            "SoccerSimulatorConfig Update",
            [this](const Configuration<SoccerSimulatorConfig>& config) {
            UpdateConfiguration(config);
        });

        // Update world state
        static constexpr float UPDATE_FREQUENCY = 100;

        on<
            Trigger<Every<UPDATE_FREQUENCY, Per<std::chrono::seconds>>>,
            With<WalkCommand>,
            With<KickCommand> 
        >("Robot motion", [this](const time_t&,
                                 const WalkCommand& walkCommand
                                 const KickCommand& kickCommand) {

            FieldPose previousRobotPose = robot_pose;
            
            switch (cfg_.robot.motion_type){
                case MotionType::NONE: {
                    robot_velocity_ = arma::vec3({ 0, 0 , 0});
                    break;
                }
                case MotionType::PATH: {
                    auto t = absolute_time();
                    double period = cfg_.robot.path.period;
                    double x_amp = cfg_.robot.path.x_amp;
                    double y_amp = cfg_.robot.path.y_amp;

                    
                    arma::vec2 old_pos = arma::vec2(robot_position_);
                    robot_position_ = getPath(robot.path.type) % arma::vec2({ x_amp, y_amp });

                    arma::vec2 diff = robot_position_ - old_pos;

                    robot_heading_ = vectorToBearing(arma::vec2(diff));//Robot faces in the direction of movement
                    robot_velocity_ = arma::vec3({arma::norm(diff) * UPDATE_FREQUENCY, 0, 0}); //Robot coordinates
                }
                case MotionType::MOTION:
                    arma::vec2 old_pos = arma::vec2(robot_position_);
                    robot_position_ += walkCommand.rows(0,1);
                    arma::vec2 diff = robot_position_ - old_pos;
                    
                    robot_heading_ += walkCommand[2];
                    robot_velocity_ = walkCommand; //Robot coordinates

                    robotPose += walkCommand / UPDATE_FREQUENCY;
                    break;
                    
            // Update ball position
            switch (cfg_.robot.motion_type){
                case MotionType::NONE: {
                    ball_velocity_ = { 0, 0 };
                    break;

                case MotionType::PATH:{              
                    auto t = absolute_time();
                    double period = cfg_.ball.path.period;
                    double x_amp = cfg_.ball.path.x_amp;
                    double y_amp = cfg_.ball.path.y_amp;

                    ball_position_ = getPath(ball.path.type) % arma::vec2({ x_amp, y_amp });

                    auto velocity_x = -square_wave(t, period) * ((x_amp * 4) / period);
                    auto velocity_y = -square_wave(t + (period / 4.0), period) * ((y_amp * 4) / period);
                    ball_velocity_ = { velocity_x, velocity_y };

                case MotionType::MOTION:

                    break;

            emit(computeGyro(robotPose.heading - previousRobotPose.heading));
        });

        // Simulate Vision
        on<Trigger<Every<30, Per<std::chrono::seconds>>>,
            With<Raw<Sensors>>,
            Options<Sync<SoccerSimulator>>>("Vision Simulation", [this](const time_t&, const std::shared_ptr<Sensors>& sensors) {

            if (field_description_ == nullptr) {
                NUClear::log(__FILE__, __LINE__, ": field_description_ == nullptr");
                return;
            }

            // Goal observation
            if (cfg_.simulate_goal_observations) {
                auto goals = std::make_unique<std::vector<messages::vision::Goal>>();

                // Only observe goals that are in front of the robot
                arma::vec3 goal_l_pos = {0, 0, 0};
                arma::vec3 goal_r_pos = {0, 0, 0};
                goal_l_pos.rows(0, 1) = field_description_->goalpost_yl;
                goal_r_pos.rows(0, 1) = field_description_->goalpost_yr;
                if (robot_heading_ < -M_PI * 0.5 || robot_heading_ > M_PI * 0.5) {
                    goal_l_pos.rows(0, 1) = field_description_->goalpost_bl;
                    goal_r_pos.rows(0, 1) = field_description_->goalpost_br;
                }

                if (cfg_.observe_left_goal) {
                    messages::vision::Goal goal1;
                    messages::vision::VisionObject::Measurement g1_m;
                    g1_m.position = SphericalRobotObservation(robot_position_, robot_heading_, goal_r_pos);
                    g1_m.error = arma::eye(3, 3) * 0.1;
                    goal1.measurements.push_back(g1_m);
                    goal1.measurements.push_back(g1_m);
                    goal1.side = messages::vision::Goal::Side::RIGHT;
                    if (cfg_.distinguish_left_and_right_goals) {
                        goal1.side = messages::vision::Goal::Side::RIGHT;
                    } else {
                        goal1.side = messages::vision::Goal::Side::UNKNOWN;
                    }
                    goal1.sensors = sensors;
                    goals->push_back(goal1);
                }

                if (cfg_.observe_right_goal) {
                    messages::vision::Goal goal2;
                    messages::vision::VisionObject::Measurement g2_m;
                    g2_m.position = SphericalRobotObservation(robot_position_, robot_heading_, goal_l_pos);
                    g2_m.error = arma::eye(3, 3) * 0.1;
                    goal2.measurements.push_back(g2_m);
                    goal2.measurements.push_back(g2_m);
                    if (cfg_.distinguish_left_and_right_goals) {
                        goal2.side = messages::vision::Goal::Side::LEFT;
                    } else {
                        goal2.side = messages::vision::Goal::Side::UNKNOWN;
                    }
                    goal2.sensors = sensors;
                    goals->push_back(goal2);
                }

                if (goals->size() > 0)
                    emit(std::move(goals));
            }

            // Ball observation
            if (cfg_.simulate_ball_observations) {
                auto ball_vec = std::make_unique<std::vector<messages::vision::Ball>>();

                messages::vision::Ball ball;
                messages::vision::VisionObject::Measurement b_m;
                arma::vec3 ball_pos_3d = {0, 0, 0};
                ball_pos_3d.rows(0, 1) = ball_position_;
                b_m.position = SphericalRobotObservation(robot_position_, robot_heading_, ball_pos_3d);
                b_m.error = arma::eye(3, 3) * 0.1;
                ball.measurements.push_back(b_m);
                ball.sensors = sensors;
                ball_vec->push_back(ball);

                emit(std::move(ball_vec));
            }

        });

        // Emit robot to NUbugger
        on<Trigger<Every<100, std::chrono::milliseconds>>,
           With<Mock<std::vector<messages::localisation::Self>>>,
           Options<Sync<SoccerSimulator>>>("NUbugger Output Self",
            [this](const time_t&,
                   const Mock<std::vector<messages::localisation::Self>>& mock_robots) {

            auto& robots = mock_robots.data;

            emit(graph("Actual robot position", robot_position_[0], robot_position_[1]));
            // emit(graph("Actual robot heading", robot_heading_[0], robot_heading_[1]));
            emit(graph("Actual robot heading", robot_heading_));
            emit(graph("Actual robot velocity", robot_velocity_[0], robot_velocity_[1]));

            if (robots.size() >= 1) {
                emit(graph("Estimated robot position", robots[0].position[0], robots[0].position[1]));
                emit(graph("Estimated robot heading", robots[0].heading[0], robots[0].heading[1]));
            }

            // Robot message
            if (!cfg_.emit_robot_fieldobjects)
                return;

            auto robots_msg = std::make_unique<std::vector<messages::localisation::Self>>();
            for (auto& model : robots) {
                robots_msg->push_back(model);
            }

            messages::localisation::Self self_marker;
            self_marker.position = robot_position_;
            self_marker.heading = bearingToUnitVector(robot_heading_);
            self_marker.position_cov = arma::eye(2,2) * 0.1;
            robots_msg->push_back(self_marker);

            emit(std::move(robots_msg));
        });

        // Emit ball to Nubugger
        on<Trigger<Every<100, std::chrono::milliseconds>>,
           With<Mock<messages::localisation::Ball>>,
           With<Mock<std::vector<messages::localisation::Self>>>,
           Options<Sync<SoccerSimulator>>>("NUbugger Output Ball",
            [this](const time_t&,
                   const Mock<messages::localisation::Ball>& mock_ball,
                   const Mock<std::vector<messages::localisation::Self>>& mock_robots) {
            auto& ball = mock_ball.data;
            auto& robots = mock_robots.data;

            if (robots.empty())
                return;

            arma::vec2 robot_ball_pos = RobotToWorldTransform(
                robots[0].position, robots[0].heading, ball.position);
            arma::vec2 ball_pos = RobotToWorldTransform(
                robot_position_, robot_heading_, ball.position);
            emit(graph("Estimated ball position", ball_pos[0], ball_pos[1]));
            // emit(graph("Estimated ball velocity", state[2], state[3]));
            emit(graph("Actual ball position", ball_position_[0], ball_position_[1]));
            emit(graph("Actual ball velocity", ball_velocity_[0], ball_velocity_[1]));

            // Ball message
            if (!cfg_.emit_ball_fieldobjects)
                return;

            auto balls_msg = std::make_unique<std::vector<messages::localisation::Ball>>();

            // True ball position:
            messages::localisation::Ball ball_marker;
            ball_marker.position = ball_position_;
            ball_marker.velocity = ball_velocity_;
            ball_marker.position_cov = arma::eye(2,2) * 0.1;
            ball_marker.world_space = true;
            balls_msg->push_back(ball_marker);

            messages::localisation::Ball ball_model;
            ball_model.position = ball_pos;
            ball_model.velocity = ball_velocity_;
            ball_model.position_cov = ball.position_cov;
            ball_model.world_space = true;
            balls_msg->push_back(ball_model);

            // messages::localisation::Ball robot_ball;
            // robot_ball.position = robot_ball_pos;
            // robot_ball.velocity = ball_velocity_;
            // robot_ball.position_cov = arma::eye(2,2) * 0.1;
            // robot_ball.world_space = true;
            // balls_msg->push_back(robot_ball);

            emit(std::move(balls_msg));
        });
    }

    std::unique_ptr<Gyroscope> SoccerSimulator::computeGyro(float dHeading){
        std::make_unique<Gyroscope> g();
        g->x = 0;
        g->y = 0;
        g->z = dHeading;
        return std::move(g);
    }

    arma::vec2 SoccerSimulator::getPath(PathType p){
        double wave1;
        double wave2;
        switch(p){
            case PathType::SIN:
                wave1 = sine_wave(t, period);
                wave2 = sine_wave(t + (period / 4.0), period);                      
            case PathType::TRIANGLE:
                wave1 = triangle_wave(t, period);
                wave2 = triangle_wave(t + (period / 4.0), period);
        }
        return arma::vec2({wave1,wave2});
    }

}
}

