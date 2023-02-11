#ifndef FUNCTIONS_H
#define FUNCTIONS_H


#include <opencv2/core.hpp>
#include <cstdio>
#include <iostream>
#include <string.h>
#include <chrono>
#include <math.h> 
#include <vector>
#include <windows.h>
#include <Windows.h>

#include "wallClasses.h"
#include "variables.h"

using namespace cv;

uintptr_t GetBaseAddress(DWORD procId, const wchar_t* modName);
Mat getMat(HWND hWND);
int compareAngles(double bestAng, double playerAng, std::string* dir);
void detectWalls(Mat& empty, Point pt1, Point pt2, Point middle, Variables *vars);
void checkAllDetections(std::vector<Vec4i> lines, Mat& empty, Variables *vars);
void setPlayerLocation(Variables *vars);
void setWallSegments(Variables *vars);
void chooseBestAngle(std::vector<wallSegment>& wallSegments, int playerAngle, double* bestAngle);
void bestDirection(Variables *vars, Mat& outputWindow);

#endif