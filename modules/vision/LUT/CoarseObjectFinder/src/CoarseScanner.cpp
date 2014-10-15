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

#include "CoarseScanner.h"
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "utility/vision/geometry/screen.h"
#include "utility/vision/geometry/sphere.h"
#include "utility/vision/geometry/cylinder.h"



namespace modules {
    namespace vision {

        using messages::input::Image;
        using utility::vision::geometry::camTiltMatrix;
        using utility::vision::geometry::bulkRay2Pixel;
        using utility::vision::geometry::bulkPixel2Ray;
        using utility::vision::geometry::trimToFOV;
        
        /*CoarseScanner(Config& config) {
            
            //XXX: do config values
        }*/
        
        arma::mat CoarseScanner::generateAboveHorizonRays(const Image& image) {
            
            //get the max possible FOV, and the estimated pixel size
            double maxFOV = 0.0;
            double pixelSize = 0.0;
            if (image.lens.type == Image::Lens::Type::RADIAL) {
                maxFOV  = image.lens.parameters.radial.radialFOV;
                pixelSize = image.lens.parameters.radial.pixelPitch;
            } else if (image.lens.type == Image::Lens::Type::EQUIRECTANGULAR) {
                maxFOV = arma::norm(arma::vec2({image.lens.parameters.equirectangular.FOV[0], image.lens.parameters.equirectangular.FOV[1]}));
                pixelSize = maxFOV/arma::norm(arma::vec2({double(image.dimensions[0]), double(image.dimensions[1])}));
            }
            
            //work out our limits - this might need to be replaced by something simpler
            double angleLimit = std::min(maxFOV, M_PI);
            angleLimit = std::min(angleLimit, 
                         utility::vision::geometry::cylinder::arcSizeFromTopRayVertical(
                                    arma::vec3({cos(M_PI/2),0.0,sin(M_PI/2)}), 
                                    arma::vec2({MIN_POST_WIDTH,MIN_POST_HEIGHT}), 
                                    CAMERA_HEIGHT)[1]);
            
            //create our ray matrix
            arma::mat scanRays(0,0);
            
            //define the starting angle to scan from
            double offset = 0.0;
            arma::vec2 halfArcSize = arma::vec2({0.0,0.0});
            
            //loop through creating new rays
            //for loops everywhere, to make Trent proud
            for (double startAngle = 0.0 + pixelSize*MIN_SIZE_PIXELS/2; startAngle < angleLimit; startAngle += halfArcSize[1]) {
                arma::vec3 camRay = arma::vec3({cos(startAngle),0.0,sin(startAngle)});
                halfArcSize = utility::vision::geometry::cylinder::arcSizeFromBaseRayVertical(
                                    arma::vec3({cos(M_PI/2.0),0.0,sin(M_PI/2.0)}), 
                                    arma::vec2({MIN_POST_WIDTH,MIN_POST_HEIGHT}),
                                    CAMERA_HEIGHT)/2.0;
                //scale because we're mapping in spherical coordinates here
                double scaledArcWidth = halfArcSize[0];
                
                
                int numRays = int((maxFOV - offset)/scaledArcWidth);
                scanRays = arma::resize(scanRays, scanRays.n_rows + numRays, 3);
                
                double cosSA = cos(startAngle);
                double sinSA = sin(startAngle);
                
                for (int i = 0; i < numRays; ++i) {
                    scanRays.row(scanRays.n_rows - i - 1) = arma::vec3({ cos(double(i)*scaledArcWidth - maxFOV/2.0 + offset) * cosSA, 
                                                                         sin(double(i)*scaledArcWidth - maxFOV/2.0 + offset) * cosSA, 
                                                                         sinSA}).t();
                }
                
            }
            
            return scanRays;
        }
        
        arma::mat CoarseScanner::generateBelowHorizonRays(const Image& image) {
            //get the max possible FOV, and the estimated pixel size
            double maxFOV = 0.0;
            double pixelSize = 0.0;
            if (image.lens.type == Image::Lens::Type::RADIAL) {
                maxFOV  = image.lens.parameters.radial.radialFOV;
                pixelSize = image.lens.parameters.radial.pixelPitch;
            } else if (image.lens.type == Image::Lens::Type::EQUIRECTANGULAR) {
                maxFOV = arma::norm(arma::vec2({image.lens.parameters.equirectangular.FOV[0], image.lens.parameters.equirectangular.FOV[1]}));
                pixelSize = maxFOV/arma::norm(arma::vec2({double(image.dimensions[0]), double(image.dimensions[1])}));
            }
            
            //work out our limits - this might need to be replaced by something simpler
            double angleLimit = std::min(maxFOV, M_PI);
            angleLimit = std::min(angleLimit, 
                         utility::vision::geometry::sphere::arcSizeFromTopRay(
                                    arma::vec3({cos(M_PI/2),0.0,sin(M_PI/2)}), 
                                    MIN_GROUNDOBJ_SIZE, 
                                    CAMERA_HEIGHT));
            
            //create our ray matrix
            arma::mat scanRays(0,0);
            
            //define the starting angle to scan from
            double offset = 0.0;
            double halfArcSize = 0.0;
            
            //loop through creating new rays
            //for loops everywhere, to make Trent proud
            for (double startAngle = 0.0 + pixelSize*MIN_SIZE_PIXELS/2; startAngle < angleLimit; startAngle += halfArcSize) {
                arma::vec3 camRay = arma::vec3({cos(startAngle),0.0,sin(startAngle)});
                halfArcSize = utility::vision::geometry::sphere::arcSizeFromBaseRay(
                                    arma::vec3({cos(M_PI/2.0),0.0,sin(M_PI/2.0)}), 
                                    MIN_GROUNDOBJ_SIZE,
                                    CAMERA_HEIGHT)/2.0;
                //scale because we're mapping in spherical coordinates here
                double scaledArcWidth = halfArcSize;
                
                
                int numRays = int((maxFOV - offset)/scaledArcWidth);
                scanRays = arma::resize(scanRays, scanRays.n_rows + numRays, 3);
                
                double cosSA = cos(startAngle);
                double sinSA = sin(startAngle);
                
                for (int i = 0; i < numRays; ++i) {
                    scanRays.row(scanRays.n_rows - i - 1) = arma::vec3({ cos(double(i)*scaledArcWidth - maxFOV/2.0 + offset) * cosSA, 
                                                                         sin(double(i)*scaledArcWidth - maxFOV/2.0 + offset) * cosSA, 
                                                                         sinSA}).t();
                }
                
            }
            
            return scanRays;
        }
        
        //do a coarse scan for objects
        std::unordered_map<uint,std::vector<arma::ivec2>> CoarseScanner::findObjects(const messages::input::Image& image,
                               const messages::vision::LookUpTable& lut, 
                               const arma::mat& horizonNormals) {
            //world space
            arma::mat33 camTransform = camTiltMatrix(image);
            
            //x = forward tilted cam space
            arma::mat alignedTransform = camTiltMatrix(image);
            
            //get scanRays for the correct FOV
            //XXX: cache these eventually
            arma::mat aboveHorizonRays = camTransform * generateAboveHorizonRays(image);
            arma::mat belowHorizonRays = camTransform * generateBelowHorizonRays(image);
            
            //trim the scanrays using the visual horizon
            if (horizonNormals.n_elem > 0) {
                belowHorizonRays = belowHorizonRays.rows(arma::find(arma::prod(belowHorizonRays * horizonNormals.t() < 0.0, 1)));
            }
            
            //trim the scanrays to the field of view
            aboveHorizonRays = trimToFOV(aboveHorizonRays,image);
            belowHorizonRays = trimToFOV(belowHorizonRays,image);
            
            
            //convert to pixels
            arma::imat aboveHorizonPixels = arma::conv_to<arma::imat>::from(bulkRay2Pixel(aboveHorizonRays,image));
            arma::imat belowHorizonPixels = arma::conv_to<arma::imat>::from(bulkRay2Pixel(belowHorizonRays,image));
            
            
            //find all the unique pixels
            std::unordered_map<uint,std::vector<arma::ivec2>> classifiedAboveHorizon;
            std::unordered_map<uint,std::vector<arma::ivec2>> classifiedBelowHorizon;
            std::unordered_set<uint> usedPixels;
            
            //above horizon classification
            for (uint i = 0; i < aboveHorizonPixels.n_rows; ++i) {
                const uint key = aboveHorizonPixels(i,0)+aboveHorizonPixels(i,1)*image.dimensions[0];
                if (usedPixels.count(key) == 0) {
                    usedPixels.insert(key);
                    
                    //XXX: do LUT lookups
                    const uint lutcolour = 0;
                    
                    classifiedAboveHorizon[lutcolour].push_back( arma::ivec2({aboveHorizonPixels(i,0), aboveHorizonPixels(i,1)}) );
                    
                }
            }
            
            //this is not actually necessary, but should speed up lookups
            usedPixels.clear();
            
            //below horizon classification
            for (uint i = 0; i < belowHorizonPixels.n_rows; ++i) {
                const uint key = belowHorizonPixels(i,0)+belowHorizonPixels(i,1)*image.dimensions[0];
                if (usedPixels.count(key) == 0) {
                    usedPixels.insert(key);
                    
                    //XXX: do LUT lookups
                    const uint lutcolour = 0;
                    
                    classifiedBelowHorizon[lutcolour].push_back( arma::ivec2({belowHorizonPixels(i,0), belowHorizonPixels(i,1)}) );
                    
                }
            }
            
            //XXX: define a message type to return
            return classifiedBelowHorizon;
        }

    }  // vision
}  // modules