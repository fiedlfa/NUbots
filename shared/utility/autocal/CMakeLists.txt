SET(dependencies 
	ARMADILLO
	AUTOCALIBRATION_PROJECT
	)

ADD_UTILITY_LIBRARY(${CMAKE_CURRENT_SOURCE_DIR} ${dependencies})

