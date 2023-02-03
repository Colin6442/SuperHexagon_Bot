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
#include <tlhelp32.h>
#include <tchar.h>

#include "wallClasses.h"
#include "circularList.h"

using namespace cv;
constexpr auto π = 3.14159265;

struct Offset{
	enum : DWORD {
		GameBase = 0,
		BasePointer = 0xe0000 + 0x15E8EC,  // fix, not static base

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
};

// https://www.unknowncheats.me/forum/programming-for-beginners/315168-please-explain-base-address-automatically.html
uintptr_t GetBaseAddress(DWORD procId, const wchar_t* modName){
	uintptr_t modBaseAddr = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	if (hSnap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry)) {
			do {
				if (!_wcsicmp(modEntry.szModule, modName)) {
					modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
	CloseHandle(hSnap);
	return modBaseAddr;
}

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

void setPlayerLocation(Point2d* playerXY, double* playerAngleRad, int* playerAngleDeg, float* playerOffset, String dir, HANDLE hProcess, DWORD appBase, int width, int height) {

	circularList adjustedAngle(*playerAngleDeg, 360);
	adjustedAngle += 5*(*playerOffset) + 27; *playerAngleDeg = adjustedAngle.current;

	*playerAngleRad = (double)adjustedAngle.current * π/180;

	playerXY->x = (/*100*(width/1250)*/60)*cos(*playerAngleRad) + width/2;
	playerXY->y = (/*100*(height/805)*/60)*sin(-*playerAngleRad) + height/2;

}

void setWallSegments(std::vector<wallSegment>& wallSegments, std::vector<angleDistance> angles, int* sectionAngles, double playerAngleRad, int width, int height){
	
	wallSegment wall;
	wallSegment firstWall;
	wallSegment lastWall;
	// int beginAngle = 0;
	// int prevDist = int(angles[0].getClosestWall());
	// for(int i = 0; i < 360; i++){
	// 	if(int(angles[i].getClosestWall()) != prevDist){
	// 		wall = *(new wallSegment(beginAngle, i, playerAngleRad, angles[beginAngle].getClosestWall()));
	// 		wallSegments.push_back(wall);
	// 		beginAngle = i;
	// 		prevDist = int(angles[i].getClosestWall());
	// 	}
	// }
	
	// edge cases between 0 & 360 angles
	if(sectionAngles[0] == 0){
		sectionAngles[0] += 1;
	}
	double avg = 0;

	// 0 -> sectionAngles[0]
	for(int i = 0; i < sectionAngles[0]; i++){
		avg += angles[i].getClosestWall();
	}

	// sectionAngles[5] -> 359
	for(int i = sectionAngles[5]; i < 360; i++){
		avg += angles[i].getClosestWall();
	}

	firstWall = *(new wallSegment(0, sectionAngles[0], playerAngleRad, avg/60));
	// wallSegments.push_back(wall);
	lastWall = *(new wallSegment(sectionAngles[5], 359, playerAngleRad, avg/60));
	// wallSegments.push_back(wall);

	// everything inbetween
	for(int i = 0; i < 5; i++){
		double avg = 0;
		for(int j = sectionAngles[i]; j < sectionAngles[i+1] && j < 360; j++){
			avg += angles[j].getClosestWall();
		}

		// assert(avg/double(sectionAngles[i+(6-useAll)] - sectionAngles[i]) >= 0 && avg/double(sectionAngles[i+(6-useAll)] - sectionAngles[i]) < 10000);
		if (avg / double(sectionAngles[i + 1] - sectionAngles[i]) < 0 || avg / double(sectionAngles[i + 1] - sectionAngles[i]) > 90000) {
			std::printf("unexpected: %f\n", avg / double(sectionAngles[i + 1] - sectionAngles[i]));
		}

		//																										normally +1 can be +0
		wall = *(new wallSegment(sectionAngles[i], sectionAngles[i+1], playerAngleRad, avg/double(sectionAngles[i+1] - sectionAngles[i])));
		wallSegments.push_back(wall);
	}
	
	wallSegments.push_back(firstWall);
	wallSegments.push_back(lastWall);

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
