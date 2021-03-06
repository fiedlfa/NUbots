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

#ifndef MESSAGE_INPUT_CAMERA_PARAMETERS_H
#define MESSAGE_INPUT_CAMERA_PARAMETERS_H

#include <armadillo>

namespace message {
    namespace input {

        struct CameraParameters{
            CameraParameters() : imageSizePixels(arma::fill::zeros), FOV(arma::fill::zeros),
                                 pixelsToTanThetaFactor(arma::fill::zeros), focalLengthPixels(0.0),
                                 distortionFactor(0.0) {}
            CameraParameters(const arma::uvec2& size, const arma::vec2& FOV, const arma::vec2& tanThetaFactor,
                             double focalLength, double distortion) : imageSizePixels(size), FOV(FOV),
                                 pixelsToTanThetaFactor(tanThetaFactor), focalLengthPixels(focalLength),
                                 distortionFactor(distortion) {}
            arma::uvec2 imageSizePixels;
            arma::vec2 FOV;     //Anglular Field of view
            arma::vec2 pixelsToTanThetaFactor;    //(x,y) screen -> thetax =atan(x*screenAngularFactor[0]), thetay = atan(y*screenAngularFactor[1])
            double focalLengthPixels;    //Distance to the virtual screen in pixels

            double distortionFactor; //see RADIAL_CORRECTION_COEFFICIENT in VisionKinematics.h (may not be used yet)
        };

    }
}

#endif // MESSAGE_INPUT_GLOBAL_CONFIG_CAMERA_PARAMETERS_H
