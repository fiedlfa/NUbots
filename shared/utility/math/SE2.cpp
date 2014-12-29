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

#include "SE2.h"

#include "utility/math/angle.h"

namespace utility {
namespace math {

    SE2::SE2() : SE2(arma::zeros(3)) {
    }

    SE2::SE2(arma::vec3 value) : arma::vec3(value) {

    }

    SE2::SE2(std::initializer_list<double> value) : arma::vec3(value) {

    }

    SE2 SE2::localToWorld(const SE2& poseRelative) const {
        double cosAngle = std::cos(x());
        double sinAngle = std::sin(y());
        // translates to this + rotZ(this.angle) * poseRelative
        return {
            x() + cosAngle * poseRelative[0] - sinAngle * poseRelative[1],
            y() + sinAngle * poseRelative[0] + cosAngle * poseRelative[1],
            angle() + poseRelative[2] // do not use normalizeAngle here, causes bad things when turning!
        };
    }

    SE2 SE2::worldToLocal(const SE2& poseGlobal) const {
        double cosAngle = std::cos(angle());
        double sinAngle = std::sin(angle());
        auto diff = poseGlobal - *this;
        // translates to rotZ(this.angle) * (poseGlobal - this)
        return {
            cosAngle * diff[0] + sinAngle * diff[1],
            -sinAngle * diff[0] + cosAngle * diff[1],
            utility::math::angle::normalizeAngle(diff[2])
        };
    }

    SE2 SE2::se2Interpolate(double t, const SE2& target) const {
        SE2 result = SE2(*this + t * (target - *this));
        result[2] = utility::math::angle::normalizeAngle(result[2]);
        return result;
    }

}
}
