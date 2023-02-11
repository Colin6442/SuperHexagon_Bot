#include <vector>

#include "wallClasses.h"
#include "variables.h"

void Variables::resetInLoopVars(){
	angles.clear();

	parallelWalls.clear();

	wallSegments.clear();

	// delete sectionAngles;
	for(int i = 0; i < 6; i++){
		sectionAngles[i] = 0;
	}
}