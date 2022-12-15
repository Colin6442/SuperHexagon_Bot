#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#include <cstdio>
#include <iostream>
#include <string.h>
#include <chrono>
#include <math.h> 
#include <vector>
#include <windows.h>
#include <Windows.h>

#include "wallClasses.h"
#include "circularList.h"

using namespace cv;
constexpr auto π = 3.14159265;

struct Offset {
	enum : DWORD {
		GameBase = 0,
		BasePointer = 0x380000 + 0x15E8EC,  // fix, not static base

		PlayerAngle = 0x1E0,
		relativeAngle = 0x2980,
		MouseDownLeft = 0x38,
		MouseDownRight = 0x3C,
		otherAngle1 = 0x1D8,
		otherAngle2 = 0x1DC,
		otherAngle3 = 0x1AC,
	};
};

Mat getMat(HWND hWND) {

	HDC deviceContext = GetDC(hWND);
	HDC memoryDeviceContext = CreateCompatibleDC(deviceContext);

	RECT windowRect;
	GetClientRect(hWND, &windowRect);

	int height = windowRect.bottom;
	int width = windowRect.right;

	HBITMAP bitmap = CreateCompatibleBitmap(deviceContext, width, height);

	SelectObject(memoryDeviceContext, bitmap);

	//copy data into bitmap
	BitBlt(memoryDeviceContext, 0, 0, width, height, deviceContext, 0, 0, SRCCOPY);


	//specify format by using bitmapinfoheader!
	BITMAPINFOHEADER bi;
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = width;
	bi.biHeight = -height;
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 1;
	bi.biYPelsPerMeter = 2;
	bi.biClrUsed = 3;
	bi.biClrImportant = 4;

	Mat mat = Mat(height, width, CV_8UC4); //8 bit unsigned ints 4 Channels -> RGBA

	//transform data and store into mat.data
	GetDIBits(memoryDeviceContext, bitmap, 0, height, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

	//clean up!
	DeleteObject(bitmap);
	DeleteDC(memoryDeviceContext); //delete not release!
	ReleaseDC(hWND, deviceContext);

	return mat;
}

int compareAngles(double bestAng, double playerAng, String* dir){
	double normal = abs(playerAng*(180/π) - bestAng*(180/π));
	double add360 = playerAng*(180/π) - bestAng*(180/π) + 360;
	double minus360 = abs(playerAng*(180/π) - bestAng*(180/π) - 360);

	if(abs(int(playerAng*(180/π)) - bestAng*(180/π)) < 2){
		*dir = "None";
		return 0;
	}

	if(normal < add360 && normal < minus360){
		if(playerAng*(180/π) - bestAng*(180/π) > 0){
			*dir = "Right";
		}else{
			*dir = "Left";
		}
		return abs(normal);
	}else if(add360 < normal && add360 < minus360){
		*dir = "Right";
		return add360;
	}else if(minus360 < normal && minus360 < add360){
		*dir = "Left";
		return minus360;
	}
	return 0;
	
}

void detectWalls(std::vector<Wall>& parallelWalls, Mat& empty, std::vector<angleDistance>& angles, Point pt1, Point pt2, Point middle, int width, int height) {
	std::vector<Point> triangle;
	if((pt1.x - pt2.x) != 0){
		double slope = double(pt1.y - pt2.y)/double(pt1.x - pt2.x);
		double yOffSet = (slope*(width/2) - slope*pt1.x + pt1.y);
		double toCenter = sqrt(pow(middle.x - width*.5, 2) + pow(middle.y - height*.5, 2));
		if (sqrt(pow(yOffSet - height/2, 2)) < 50){ // parallel walls
			// line(empty, pt1, pt2, Scalar(0, 200, 0), 1, LINE_AA);
		}else{ // perpendicular walls
			Wall added = *(new Wall(pt1, pt2, slope, toCenter, middle));
			parallelWalls.push_back(added);
			line(empty, pt1, pt2, Scalar(160, 0, 160), 1, LINE_AA);

			// putText(empty, std::to_string((int)pt1Angle), pt1, FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
			// putText(empty, std::to_string((int)pt2Angle), pt2, FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
			// std::printf("%f\n",pt1Angle);
		}
	}

	// double fromCenter = sqrt(pow(middle.x - width*.5, 2) + pow(middle.y - height*.5, 2));
}

void checkAllDetections(std::vector<Vec4i> lines, std::vector<Wall>& parallelWalls, std::vector<angleDistance>& angles, Mat& empty, int width, int height){
	double length;
	for(size_t i = 0; i < lines.size(); i++) {
		Vec4i line = lines[i];
		Point pos = Point(abs(line[0] + line[2]) / 2, abs(line[1] + line[3]) / 2);

		if(pos.y > 70 && sqrt(pow(pos.x - width*.5, 2) + pow(pos.y - height*.5, 2)) > 120){
			detectWalls(parallelWalls, empty, angles, Point(line[0], line[1]), Point(line[2], line[3]), pos, width, height);
			// circle(empty, pos, 4, Scalar(255, 255, 0), 6, 8, 0);
		}
	}
}

void setPlayerLocation(Point2d* playerXY, double* playerAngleRad, int* playerAngleDeg, HANDLE hProcess, DWORD appBase, int width, int height) {
	bool success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(appBase + Offset::PlayerAngle), playerAngleDeg, sizeof(DWORD), NULL);
	assert(success);
	circularList adjustedAngle(*playerAngleDeg, 359);
	adjustedAngle += 27; *playerAngleDeg = adjustedAngle.current;
	*playerAngleRad = (double)adjustedAngle.current * π/180;

	playerXY->x = 100*cos(*playerAngleRad) + width/2;
	playerXY->y = 100*sin(-*playerAngleRad) + height/2;

}

void setWallSegments(std::vector<wallSegment>& wallSegments, std::vector<angleDistance> angles, Mat& empty, double playerAngleRad, int width, int height){
	
	int beginAngle = 0;
	int prevDist = int(angles[0].getClosestWall());
	wallSegment wall;
	for(int i = 0; i < 360; i++){
		if(int(angles[i].getClosestWall()) != prevDist){
			wall = *(new wallSegment(beginAngle, i, playerAngleRad, angles[beginAngle].getClosestWall()));
			wallSegments.push_back(wall);
			beginAngle = i;
			prevDist = int(angles[i].getClosestWall());
		}
	}

}

void chooseBestAngle(std::vector<wallSegment>& wallSegments, int playerAngle, double* bestAngle){
	int farthest = 0;
	int index = 0;
	for(int i = 0; i < wallSegments.size(); i++){
		if(wallSegments[i].distToCenter > farthest){
			farthest = wallSegments[i].distToCenter;
			index = i;
		}
	}

	*bestAngle = wallSegments[index].middle;


}
