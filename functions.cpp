#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#include <cstdio>
#include <cstdlib>
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
#include "variables.h"

using namespace cv;
constexpr auto π = 3.14159265;

struct Offset{
	enum : DWORD {
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

int compareAngles(double bestAng, double playerAng, std::string* dir){
	double normal = abs(playerAng - bestAng);
	double add360 = playerAng - bestAng + 360;
	double minus360 = abs(playerAng - bestAng - 360);

	if(abs(int(playerAng) - bestAng) < 2){
		*dir = "None";
		return 0;
	}

	if(normal < add360 && normal < minus360){
		if(playerAng - bestAng > 0){
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

void detectWalls(Mat& empty, Point pt1, Point pt2, Point middle, Variables *vars) {
	std::vector<Point> triangle;
	if((pt1.x - pt2.x) != 0){
		double slope = double(pt1.y - pt2.y)/double(pt1.x - pt2.x);
		double yOffSet = (slope*(vars->width/2) - slope*pt1.x + pt1.y);
		double toCenter = sqrt(pow(middle.x - vars->width*.5, 2) + pow(middle.y - vars->height*.5, 2));
		if (sqrt(pow(yOffSet - vars->height/2, 2)) < 50){ // parallel walls
			// line(empty, pt1, pt2, Scalar(0, 200, 0), 1, LINE_AA);
		}else{ // perpendicular walls
			Wall added = *(new Wall(pt1, pt2, slope, toCenter, middle));
			vars->parallelWalls.push_back(added);
			// delete &added;
			line(empty, pt1, pt2, Scalar(160, 0, 160), 1, LINE_AA);

			// putText(empty, std::to_string((int)pt1Angle), pt1, FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
			// putText(empty, std::to_string((int)pt2Angle), pt2, FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
			// std::printf("%f\n",pt1Angle);
		}
	}

	// double fromCenter = sqrt(pow(middle.x - width*.5, 2) + pow(middle.y - height*.5, 2));
}

void checkAllDetections(std::vector<Vec4i> lines, Mat& empty, Variables *vars){
	double length;
	for(size_t i = 0; i < lines.size(); i++) {
		Vec4i line = lines[i];
		Point pos = Point(abs(line[0] + line[2]) / 2, abs(line[1] + line[3]) / 2);

		// L: 0+210 x 33  || R: 434+335 x 53
		if( !((pos.x > 434 && pos.y < 53) || (pos.x < 210 && pos.y < 33)) ){
			if(sqrt(pow(pos.x - vars->width*.5, 2) + pow(pos.y - vars->height*.5, 2)) > 60){
				detectWalls(empty, Point(line[0], line[1]), Point(line[2], line[3]), pos, vars);
				// circle(empty, pos, 4, Scalar(255, 255, 0), 6, 8, 0);
			}
		}
	}
}

void setPlayerLocation(Variables *vars) {

	circularList adjustedAngle(vars->playerAngleDeg, 360);
	adjustedAngle += 5*(vars->playerOffset) + 27; vars->playerAngleDeg = adjustedAngle.current;

	vars->playerAngleRad = (double)adjustedAngle.current * π/180;

	vars->playerXY.x = (/*100*(width/1250)*/60)*cos(vars->playerAngleRad) + vars->width/2;
	vars->playerXY.y = (/*100*(height/805)*/60)*sin(-vars->playerAngleRad) + vars->height/2;

}

void setWallSegments(Variables *vars){
	
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
	if(vars->sectionAngles[0] == 0){
		vars->sectionAngles[0] += 1;
	}
	double avg = 0;

	// 0 -> sectionAngles[0]
	for(int i = 0; i < vars->sectionAngles[0]; i++){
		avg += vars->angles[i].getClosestWall();
	}

	// sectionAngles[5] -> 359
	for(int i = vars->sectionAngles[5]; i < 360; i++){
		avg += vars->angles[i].getClosestWall();
	}

	circularList middle(vars->sectionAngles[5], 360);
	middle += 30;

	firstWall = *(new wallSegment(0, vars->sectionAngles[0], middle.current, vars->playerAngleDeg, avg/60));
	// wallSegments.push_back(wall);
	lastWall = *(new wallSegment(vars->sectionAngles[5], 359, middle.current, vars->playerAngleDeg, avg/60));
	// wallSegments.push_back(wall);

	// everything inbetween
	for(int i = 0; i < 5; i++){
		double avg = 0;
		for(int j = vars->sectionAngles[i]; j < vars->sectionAngles[i+1] && j < 360; j++){
			avg += vars->angles[j].getClosestWall();
		}

		// assert(avg/double(sectionAngles[i+(6-useAll)] - sectionAngles[i]) >= 0 && avg/double(sectionAngles[i+(6-useAll)] - sectionAngles[i]) < 10000);
		if (avg / double(vars->sectionAngles[i + 1] - vars->sectionAngles[i]) < 0 || avg / double(vars->sectionAngles[i + 1] - vars->sectionAngles[i]) > 90000) {
			std::printf("unexpected: %f\n", avg / double(vars->sectionAngles[i + 1] - vars->sectionAngles[i]));
		}

		//																										normally +1 can be +0
		wall = *(new wallSegment(vars->sectionAngles[i], vars->sectionAngles[i+1], (vars->sectionAngles[i+1] + vars->sectionAngles[i])/2, vars->playerAngleDeg, avg/double(vars->sectionAngles[i+1] - vars->sectionAngles[i])));
		vars->wallSegments.push_back(wall);
		// delete &wall;
	}
	
	vars->wallSegments.push_back(lastWall);
	vars->wallSegments.push_back(firstWall);
	

	for(int i = 0; i < 6; i++){
		for(int j = 0; j < 6; j++){
			if(abs(vars->wallSegments[i].distToCenter - vars->wallSegments[j].distToCenter) < 10){
				int avg = (vars->wallSegments[i].distToCenter + vars->wallSegments[j].distToCenter) / 2;
				vars->wallSegments[i].distToCenter = avg;
				vars->wallSegments[j].distToCenter = avg;
			}
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

void bestDirection(Variables *vars, Mat& outputWindow){
	int bestSlot = 0;
	int bestScore = 0;
	
	for(int i = 0; i < 6; i++){
		int scoring = vars->wallSegments[i].distToCenter;
		scoring -= vars->wallSegments[i].angleFromPlayer;
		if(vars->playerSlot != i && vars->wallSegments[vars->playerSlot].distToCenter < 110){
			scoring += 80;
		}
		if(vars->playerSlot != i && vars->wallSegments[vars->playerSlot].distToCenter < 100){
			scoring -= 50;
		}
		
		// <120 dont go


		// if(vars->wallSegments[i].dirFromPlayer == "Left"){
		// 	for(int j = vars->playerSlot; j != i; j++){
		// 		if(j == 6){ j = 0;}
		// 		if(j == 0 && i == 0){break;}
		// 		if(vars->wallSegments[j].distToCenter < 150){
		// 			scoring -= 150 - vars->wallSegments[j].distToCenter;
		// 		}
		// 	}
		// }

		// if(vars->wallSegments[i].dirFromPlayer == "Right"){
		// 	for(int j = vars->playerSlot; j != i; j--){
		// 		if(j == -1){ j = 5;}
		// 		if(j == 5 && i == 5){break;}
		// 		if(vars->wallSegments[j].distToCenter < 150){
		// 			scoring -= 150 - vars->wallSegments[j].distToCenter;
		// 		}
		// 	}
		// }

		if(scoring > bestScore){
			bestScore = scoring;
			bestSlot = i;
		}

		vars->wallSegments[i].score = scoring;
	}

	int dir = 0;
	if(vars->wallSegments[bestSlot].dirFromPlayer == "Left"){ dir = 1;}
	if(vars->wallSegments[bestSlot].dirFromPlayer == "Right"){ dir = -1;}

	if(dir != 0){
		int i = vars->playerSlot;
		while(i != bestSlot){
			i += dir;
			if(i == vars->playerSlot){ i += dir;}
			if(i == 6){ i = 0;}
			if(i == -1){ i = 5;}
			
			// std::printf("best: %d, i: %d\n", bestSlot, i);
			if(vars->wallSegments[i].distToCenter < 110){
				vars->wallSegments[vars->playerSlot].score += 60;
				break;
			}


		}
	}

	bestSlot = 0;
	bestScore = 0;
	for(int i = 0; i < 6; i++){
		if(vars->wallSegments[i].score > bestScore){
			bestScore = vars->wallSegments[i].score;
			bestSlot = i;
		}
	}

	// std::printf("-------------------\n");

	vars->best_slot = bestSlot;

	vars->dir = vars->wallSegments[bestSlot].dirFromPlayer;




}