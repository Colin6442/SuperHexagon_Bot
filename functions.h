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

using namespace cv;

Mat getMat(HWND hWND);
int compareAngles(double bestAng, double playerAng, String* dir);
void detectWalls(std::vector<Wall>& parallelWalls, Mat& empty, std::vector<angleDistance>& angles, Point pt1, Point pt2, Point middle, int width, int height);
void checkAllDetections(std::vector<Vec4i> lines, std::vector<Wall>& parallelWalls, std::vector<angleDistance>& angles, Mat& empty, int width, int height);
void setPlayerLocation(Point2d* playerXY, double* playerAngleRad, int* playerAngleDeg, HANDLE hProcess, DWORD appBase, int width, int height);
void setWallSegments(std::vector<wallSegment>& wallSegments, std::vector<angleDistance> angles, Mat& empty, double playerAngleRad, int width, int height);
void chooseBestAngle(std::vector<wallSegment>& wallSegments, int playerAngle, double* bestAngle);

#endif