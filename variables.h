#pragma once
#ifndef VARS_H
#define VARS_H

#include <opencv2/core.hpp>
#include <string.h>
#include <vector>
#include <windows.h>
#include <Windows.h>


#include "wallClasses.h"

struct Variables{

	enum Offsets : DWORD {
		BaseOffset = 0x15E8EC,
		PlayerAngle = 0x1E0,
		PlayerAngleOffset = 0x298C, //float
		relativeAngle = 0x2980,
		MouseDownLeft = 0x38,
		MouseDownRight = 0x3C,
		worldAngle = 0x1A0,			//float
		otherAngle1 = 0x1D8,
		otherAngle2 = 0x1DC,
		otherAngle3 = 0x1AC,
		otherAngle4 = 0x2974,
		otherAngle5 = 0x2978,
	};
	
	// Out of Loop Variables
	HANDLE hProcess;
	DWORD appBase;
	int count = 0;
	cv::Point2d playerXY = cv::Point2d(0,0);
	double playerAngleRad = 0;
	int playerAngleDeg = 0;
	int playerSlot = 0;
	int best_slot = 0;
	std::string dir = "None";
	int width;
	int height;
	float playerOffset;
	int relativeAngles;

	// In Loop Variables
	std::vector<angleDistance> angles;
	std::vector<Wall> parallelWalls;
	std::vector<wallSegment> wallSegments;
	int* sectionAngles = new int[6];


	// Variable functions
	void resetInLoopVars();

};

#endif