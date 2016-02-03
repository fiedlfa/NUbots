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

#include "VisualMesh.h"
#include "message/input/Image.h"
#include "message/input/Sensors.h"
#include "message/vision/MeshObjectRequest.h"
#include "message/support/Configuration.h"
#include "message/motion/ServoTarget.h"

#include "utility/math/matrix/Rotation3D.h"
#include "utility/support/yaml_armadillo.h"
#include "utility/nubugger/NUhelpers.h"

#include "utility/motion/RobotModels.h"

namespace module {
namespace vision {

    using message::input::Image;
    using message::input::Sensors;
    using message::vision::MeshObjectRequest;
    using message::support::Configuration;

    using utility::motion::kinematics::DarwinModel;
    using message::motion::ServoTarget;

    using utility::math::matrix::Rotation3D;
    using utility::nubugger::graph;

    // http://www.math-only-math.com/a-cos-theta-plus-b-sin-theta-equals-c.html
    arma::vec solveAcosThetaPlusBsinThetaEqualsC(double a, double b, double c) {
        double r = std::sqrt(a*a + b*b);
        if(std::abs(c) <= r) {
            double alpha = std::atan2(b, a);
            double beta = std::acos(c/r);
            if(beta == 0) {
                return arma::vec({ alpha + beta });
            } else {
                return arma::vec({ alpha + beta, alpha - beta });
            }
        } else {
            return arma::vec();
        }
    }

    VisualMesh::VisualMesh(std::unique_ptr<NUClear::Environment> environment)
    : Reactor(std::move(environment))
    , lut(0.1, 0.5) { // TODO make this based of the kinematics

        on<Configuration>("VisualMesh.yaml").then([this] (const Configuration& config) {
            // Use configuration here from file VisualMesh.yaml
        });

        auto sphere = std::make_unique<MeshObjectRequest>();
        sphere->type = MeshObjectRequest::SPHERE;
        sphere->radius = 0.07;
        sphere->height = 0;
        sphere->intersections = 3;
        sphere->maxDistance = 10;
        sphere->hardLimit = false;

        auto cylinder = std::make_unique<MeshObjectRequest>();
        cylinder->type = MeshObjectRequest::CYLINDER;
        cylinder->radius = 0.05;
        cylinder->height = 2;
        cylinder->intersections = 2;
        cylinder->maxDistance = 10;
        cylinder->hardLimit = false;

        auto circle = std::make_unique<MeshObjectRequest>();
        circle->type = MeshObjectRequest::CIRCLE;
        circle->radius = 0.05;
        circle->height = 0;
        circle->intersections = 2;
        circle->maxDistance = 2;
        circle->hardLimit = false;

        emit<Scope::INITIALIZE>(sphere);
        emit<Scope::INITIALIZE>(cylinder);
        emit<Scope::INITIALIZE>(circle);

        on<Trigger<MeshObjectRequest>>().then([this] (const MeshObjectRequest& request) {
            lut.addShape(request);
        });

        on<Trigger<Sensors>, Single>().then([this] (const Sensors& sensors) {

            // get field of view

            float FOV_X = 1.0472;

            float FOV_Y = 0.785398;

            // use resolution and FOV
            int camFocalLengthPixels = 10;
            // Camera height is z component of the transformation matrix
            double cameraHeight = sensors.orientationCamToGround(2, 3);
            Rotation3D camToGround = Rotation3D::createRotationZ(-sensors.orientationCamToGround.rotation().yaw()) * sensors.orientationCamToGround.rotation();


            /***************************
             * Calculate image corners *
             ***************************/

            // Get the corners of the view port in cam space
            double ymax = std::tan(FOV_X * 0.5);
            double zmax = std::tan(FOV_Y * 0.5);
            arma::mat::fixed<3,4> cornerPointsCam = {
                1,  ymax,  zmax,
                1, -ymax,  zmax,
                1, -ymax, -zmax,
                1,  ymax, -zmax
            };

            cornerPointsCam /= arma::norm(cornerPointsCam.col(0));

            // Rotate the camera points into world space
            arma::mat::fixed<3,4> cornerPointsWorld = camToGround * cornerPointsCam;

            /**************************************************
             * Calculate screen edge planes and corner angles *
             **************************************************/
            arma::cube::fixed<3,3,4> screenEdgeMatricies;
            arma::vec4 screenEdgeArcs;
            for(int i = 0; i < screenEdgeMatricies.n_slices; ++i) {
                screenEdgeMatricies.slice(i).col(0) = cornerPointsWorld.col(i);
                screenEdgeMatricies.slice(i).col(2) = arma::normalise(arma::cross(cornerPointsWorld.col(i), cornerPointsWorld.col((i + 1) % 4)));
                screenEdgeMatricies.slice(i).col(1) = arma::cross(screenEdgeMatricies.slice(i).col(2), screenEdgeMatricies.slice(i).col(0));

                arma::vec3 p = screenEdgeMatricies.slice(i).t() * cornerPointsWorld.col((i + 1) % 4);
                screenEdgeArcs[i] = std::atan2(p[1], p[0]);
            }

            /*************************
             * Calculate min/max phi *
             *************************/

            // TODO THIS DOESN'T WORK WHEN CROSSING VERTICAL AS Z COMPONENTS ARE THE SAME

            // Get the cosv value
            arma::rowvec4 phiCosV = arma::acos(-cornerPointsWorld.row(2));
            // Return the minimum and maximum values
            double minPhi = phiCosV.min();
            double maxPhi = phiCosV.max();
            // std::cout << "phiCosV = " << phiCosV << std::endl;
            // std::cout << "minPhi = " << minPhi << std::endl;
            // std::cout << "maxPhi = " << maxPhi << std::endl;

            /*************************************************************
             * Get our lookup table and loop through our phi/theta pairs *
             *************************************************************/


            emit(graph("Corner Points", arma::vec(cornerPointsWorld.col(0))));
            emit(graph("Corner Points", arma::vec(cornerPointsWorld.col(1))));
            emit(graph("Corner Points", arma::vec(cornerPointsWorld.col(2))));
            emit(graph("Corner Points", arma::vec(cornerPointsWorld.col(3))));

            auto phiIterator = lut.getLUT(cameraHeight, minPhi, maxPhi);
            std::vector<arma::vec3> camPoints;

            for(auto it = phiIterator.first; it != phiIterator.second; ++it) {

                const double& phi = it->first;
                const double& dTheta = it->second;

                /*********************************************************
                 * Calculate our min and max theta values for each plane *
                 *********************************************************/
                double sinPhi = sin(phi);
                double cosPhi = cos(phi);

                std::vector<double> thetaLimits;
                thetaLimits.reserve(4);

                for(int i = 0; i < 4; ++i) {

                    // std::cout << "i = " << i << std::endl;

                    if (minPhi < phi && phi < maxPhi) {

                        double& x = screenEdgeMatricies(0, 2, i);
                        double& y = screenEdgeMatricies(1, 2, i);
                        double& z = screenEdgeMatricies(2, 2, i);

                        arma::vec v = solveAcosThetaPlusBsinThetaEqualsC(sinPhi * x, sinPhi * y, cosPhi * z);

                        // std::cout << "x" << x << std::endl;
                        // std::cout << "y" << y << std::endl;
                        // std::cout << "z" << z << std::endl;
                        // std::cout << "v" << v.t() << std::endl;

                        if(v.n_elem == 2) {

                            arma::vec2 cosV = arma::cos(v);
                            arma::vec2 sinV = arma::sin(v);

                            arma::vec3 p1 = { cosV[0] * sinPhi, sinV[0] * sinPhi, -cosPhi };
                            arma::vec3 p2 = { cosV[1] * sinPhi, sinV[1] * sinPhi, -cosPhi };

                            arma::vec3 p1d = p1;
                            arma::vec3 p2d = p2;

                            std::cout << i << std::endl;
                            std::cout << screenEdgeMatricies.slice(i);
                            std::cout << p1.t();
                            std::cout << p2.t();

                            p1 = screenEdgeMatricies.slice(i).t() * p1;
                            p2 = screenEdgeMatricies.slice(i).t() * p2;

                            double p1V = atan2(p1[1], p1[0]);
                            double p2V = atan2(p2[1], p2[0]);

                            // Check solution 1
                            if (0 < p1V && p1V < screenEdgeArcs[i]) {
                                thetaLimits.push_back(v[0]);
                                emit(graph("Solution 1 " + std::to_string(i), p1d));
                            }

                            // Check solution 2
                            if (0 < p2V && p2V < screenEdgeArcs[i]) {
                                thetaLimits.push_back(v[1]);
                                emit(graph("Solution 2 " + std::to_string(i), p2d));
                            }

                            std::cout << p1.t();
                            std::cout << p2.t();

                            std::cout << p1V << std::endl
                                      << p2V << std::endl
                                      << screenEdgeArcs[i] << std::endl
                                      << std::endl;

                            // if(0 < p1V && p1V < aoifjeaoifjse) {

                            // }
                            // if(0 < p1V && p1V < aoifjeaoifjse) {

                            // }

                        }
                    }
                }

                std::sort(thetaLimits.begin(), thetaLimits.end());

                // std::cout << "thetaLimits.size() = " << thetaLimits.size() << std::endl;

                /***********************************
                 * Loop through our theta segments *
                 ***********************************/

                for (size_t i = 0; i < thetaLimits.size() / 2; i += 2) {
                    const double& minTheta = thetaLimits[i];
                    const double& maxTheta = thetaLimits[i + 1];

                    for (double theta = minTheta; theta < maxTheta; theta += dTheta) {

                        /************************************************************************
                         * Add this phi/theta sample point to a list to project onto the screen *
                         ************************************************************************/

                        double cosTheta = std::cos(theta);
                        double sinTheta = std::sin(theta);

                        camPoints.push_back(camToGround.i() * arma::vec3({ cosTheta * sinPhi, sinTheta * sinPhi, -cosPhi }));

                        // emit(graph("Phi Points", camPoints.back()));
                    }
                }
            }

            /***********************************************
             * Project our phi/theta pairs onto the screen *
             ***********************************************/

            //std::vector<arma::vec2> screenPoints;

            //for(auto& point : phiThetaPoints) {
            //    screenPoints.push_back(phiThetaToScreenPoint(point.first, point.second));
               // std::cout << screenPoints.back() << std::endl;
            //}
        });
    }

    arma::vec2 VisualMesh::phiThetaToScreenPoint(double phi, double theta, Rotation3D camToGround, int camFocalLengthPixels) {
        // phi and theta are converted to spherical coordinates in a space with the same orientation as the world, but with origin at the camera position.
        // The associated phi in spherical coords is given by -(pi/2 - phi), which simplifies the spherical coordinate conversion to
        arma::vec3 sphericalCoords = { std::cos(theta)*std::sin(phi), std::sin(theta)*std::sin(phi), -std::cos(phi) };
        // To put in camera space multiply by the ground to camera rotation matrix
        arma::vec3 camSpacePoint = camToGround.i() * sphericalCoords;
        // Project camera space to screen space by
        arma::vec2 screenSpacePoint = arma::vec2({camFocalLengthPixels * camSpacePoint[1] / camSpacePoint[0], camFocalLengthPixels * camSpacePoint[2] / camSpacePoint[0]});
        return screenSpacePoint;
    }
    arma::vec3 VisualMesh::phiThetaToSphericalCameraSpace(double phi, double theta, Rotation3D camToGround) {
        // phi and theta are converted to spherical coordinates in a space with the same orientation as the world, but with origin at the camera position.
        // The associated phi in spherical coords is given by -(pi/2 - phi), which simplifies the spherical coordinate conversion to
        arma::vec3 cartesianCoords = { std::cos(theta)*std::sin(phi), std::sin(theta)*std::sin(phi), -std::cos(phi) };
        // To put in camera space multiply by the ground to camera rotation matrix
        arma::vec3 camSpacePoint = camToGround.i() * cartesianCoords;
        // put back into spherical coordinates
                double x = camSpacePoint[0];
                double y = camSpacePoint[1];
                double z = camSpacePoint[2];
                arma::vec3 result;

                result[0] = sqrt(x*x + y*y + z*z);  //r
                result[1] = atan2(y, x);            //theta
                if(result[0] == 0) {
                    result[2] = 0;
                } else {
                    result[2] = asin(z / (result[0]));  //phi
                }

                return result;

    }


}
}