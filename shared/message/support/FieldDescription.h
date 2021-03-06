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

#ifndef MESSAGE_SUPPORT_FIELDDESCRIPTION_H
#define MESSAGE_SUPPORT_FIELDDESCRIPTION_H

#include <armadillo>

namespace message {namespace support {

class FieldDescription {
    public:
    FieldDescription() : dimensions(), ball_radius(0.0), goalpost_top_height(0.0), penalty_robot_start(0.0),
                         goalpost_own_l(arma::fill::zeros), goalpost_own_r(arma::fill::zeros),
                         goalpost_opp_l(arma::fill::zeros), goalpost_opp_r(arma::fill::zeros) {}

    // Field dimensions as defined in the Robocup rules:
    struct FieldDimensions {
        FieldDimensions()
            : line_width(0.0)
            , mark_width(0.0)
            , field_length(0.0)
            , field_width(0.0)
            , goal_depth(0.0)
            , goal_width(0.0)
            , goal_area_length(0.0)
            , goal_area_width(0.0)
            , goal_crossbar_height(0.0)
            , goalpost_diameter(0.0)
            , goal_crossbar_diameter(0.0)
            , goal_net_height(0.0)
            , penalty_mark_distance(0.0)
            , center_circle_diameter(0.0)
            , border_strip_min_width(0.0) {}

        double line_width;
        double mark_width;
        double field_length;
        double field_width;
        double goal_depth;
        double goal_width;
        double goal_area_length;
        double goal_area_width;
        double goal_crossbar_height;
        double goalpost_diameter;
        double goal_crossbar_diameter;
        double goal_net_height;
        double penalty_mark_distance;
        double center_circle_diameter;
        double border_strip_min_width;
    } dimensions;

    double ball_radius;
    double goalpost_top_height;
    double penalty_robot_start;

    // Coordinates of goalpost centers calculated from the FieldDimensions:
    // (arma::vec2)
    arma::vec2 goalpost_own_l;
    arma::vec2 goalpost_own_r;
    arma::vec2 goalpost_opp_l;
    arma::vec2 goalpost_opp_r;
};

}
}





#endif
