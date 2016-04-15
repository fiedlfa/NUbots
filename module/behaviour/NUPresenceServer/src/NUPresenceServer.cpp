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
 * Copyright 2016 NUbots <nubots@nubots.net>
 */

#include "NUPresenceServer.h"

#include "message/support/Configuration.h"

#include "message/input/Image.h"
#include "message/input/Sensors.h"
#include "message/input/ServoID.h"
#include "message/input/proto/ImageFragment.pb.h"
#include "utility/math/matrix/Transform3D.h"

namespace module {
namespace behaviour {

    using message::support::Configuration;

    using message::input::Image;
    using message::input::Sensors;
    using message::input::ServoID;
    using message::input::proto::ImageFragment;
    using utility::math::matrix::Transform3D;


    NUPresenceServer::NUPresenceServer(std::unique_ptr<NUClear::Environment> environment)
    : Reactor(std::move(environment)) {

        on<Configuration>("NUPresenceServer.yaml").then([this] (const Configuration& config) {
            reliable = config["reliable"];
        });

        on<Trigger<Image>, With<Sensors>>().then([this](const Image& image, const Sensors& sensors){

        	auto imageFragment = std::make_unique<ImageFragment>();

            imageFragment->set_camera_id(0);
            imageFragment->mutable_dimensions()->set_x(image.width);
            imageFragment->mutable_dimensions()->set_y(image.height);

            imageFragment->set_format(message::input::proto::ImageFragment::YCbCr422);

            // Reserve enough space in the image data to store the output
            std::string* imageBytes = imageFragment->mutable_data();
            imageBytes->reserve(image.source().size());
            imageBytes->insert(imageBytes->begin(), std::begin(image.source()), std::end(image.source()));

            imageFragment->set_start(0);
            imageFragment->set_end(image.source().size());

            Transform3D cam_to_right_foot = sensors.forwardKinematics.at(ServoID::R_ANKLE_ROLL).i() * sensors.forwardKinematics.at(ServoID::HEAD_PITCH);
            Transform3D cam_to_left_foot = sensors.forwardKinematics.at(ServoID::L_ANKLE_ROLL).i() * sensors.forwardKinematics.at(ServoID::HEAD_PITCH);
            Transform3D cam_to_feet = cam_to_left_foot;
            cam_to_feet.translation() = 0.5 * (cam_to_left_foot.translation() + cam_to_right_foot.translation());
            imageFragment->mutable_cam_to_feet()->mutable_x()->set_x(cam_to_feet(0,0));
            imageFragment->mutable_cam_to_feet()->mutable_x()->set_y(cam_to_feet(1,0));
            imageFragment->mutable_cam_to_feet()->mutable_x()->set_z(cam_to_feet(2,0));
            imageFragment->mutable_cam_to_feet()->mutable_x()->set_t(cam_to_feet(3,0));

            imageFragment->mutable_cam_to_feet()->mutable_y()->set_x(cam_to_feet(0,1));
            imageFragment->mutable_cam_to_feet()->mutable_y()->set_y(cam_to_feet(1,1));
            imageFragment->mutable_cam_to_feet()->mutable_y()->set_z(cam_to_feet(2,1));
            imageFragment->mutable_cam_to_feet()->mutable_y()->set_t(cam_to_feet(3,1));

            imageFragment->mutable_cam_to_feet()->mutable_z()->set_x(cam_to_feet(0,2));
            imageFragment->mutable_cam_to_feet()->mutable_z()->set_y(cam_to_feet(1,2));
            imageFragment->mutable_cam_to_feet()->mutable_z()->set_z(cam_to_feet(2,2));
            imageFragment->mutable_cam_to_feet()->mutable_z()->set_t(cam_to_feet(3,2));

            imageFragment->mutable_cam_to_feet()->mutable_t()->set_x(cam_to_feet(0,3));
            imageFragment->mutable_cam_to_feet()->mutable_t()->set_y(cam_to_feet(1,3));
            imageFragment->mutable_cam_to_feet()->mutable_t()->set_z(cam_to_feet(2,3));
            imageFragment->mutable_cam_to_feet()->mutable_t()->set_t(cam_to_feet(3,3));

            emit<Scope::NETWORK>(imageFragment, "nupresenceclient", reliable);

        });

        on<Trigger<NUClear::message::NetworkJoin>>().then([this](const NUClear::message::NetworkJoin& join){
        	log(join.name);
        });
    }
}
}