/*
 * This file is part of FeatureDetector.
 *
 * FeatureDetector is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FeatureDetector is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FeatureDetector.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2013 NUBots <nubots@nubots.net>
 */

#ifndef MODULES_VISION_GOALDETECTOR_RANSAC_H
#define MODULES_VISION_GOALDETECTOR_RANSAC_H

#include <vector>
#include <list>
#include <armadillo>

#include "messages/vision/ClassifiedImage.h"

#include "GoalDetector.h"
#include "Quad.h"
#include "RANSAC.h"

namesapce modules {
	namesapce vision {
	
		class GoalDetector_RANSAC : public GoalDetector {
		public:
			GoalDetector_RANSAC();
			virtual std::vector<Goal> run();

			void setParameters(unsigned int minimumPoints, 
								unsigned int maxIterationsPerFitting, 
								double consensusThreshold, 
								unsigned int maxFittingAttempts, 
								double ANGLE_MARGIN_, 
								Horizon& kinematicsHorizon, 
								RANSAC_SELECTION_METHOD RANSACMethod
								double RANSAC_MATCHING_TOLERANCE_);

		private:
			std::list<Quad> buildQuadsFromLines(const std::vector<LSFittedLine>& start_lines,  const std::vector<LSFittedLine>& end_lines, double tolerance);

			unsigned int getClosestUntriedLine(const LSFittedLine& start, const std::vector<LSFittedLine>& end_lines, std::vector<bool>& tried);

			std::vector<Goal> assignGoals(const std::list<Quad>& candidates, const Quad& crossbar) const;

			std::vector<Point> getEdgePointsFromSegments(const std::vector<ColourSegment> &segments);


			unsigned int m_minimumPoints;							// Minimum points needed to make a line (Min pts to line essentially)
			unsigned int m_maxIterationsPerFitting;					// Number of iterations per fitting attempt
			unsigned int m_maxFittingAttempts;						// Hard limit on number of fitting attempts
			
			double ANGLE_MARGIN;									// Used for filtering out goal posts which are on too much of a lean.
			double m_consensusThreshold;							// Threshold dtermining what constitutes a good fit (Consensus margin)
			
			RANSAC_SELECTION_METHOD m_RANSACMethod;
			Horizon& m_kinematicsHorizon;
			
			double RANSAC_MATCHING_TOLERANCE;
		};
		
	}
}

#endif //  MODULES_VISION_GOALDETECTOR_RANSAC_H