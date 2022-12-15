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
#include "functions.h"


constexpr auto π = 3.14159265;

using namespace cv;

typedef std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;

struct Offsets{
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


int main() {

	LPCTSTR window_title = (LPCTSTR)"Super Hexagon";
	HWND hWND = FindWindow(NULL, window_title);
	namedWindow("output", WINDOW_NORMAL);
	
	DWORD processId;
	GetWindowThreadProcessId(hWND, &processId);
	HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, processId);
	
	DWORD appBase;
	// TODO func to get base pointer  (not static on PC restarts)
	bool success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(Offsets::BasePointer), &appBase, sizeof(DWORD), NULL);
	assert(success);

	


	INPUT keyboard;
	keyboard.type = INPUT_KEYBOARD;
	keyboard.ki.time = 0;
	keyboard.ki.wVk = 0;
	keyboard.ki.dwExtraInfo = 0;
	keyboard.ki.dwFlags = 0;
	keyboard.ki.wVk = 0;


	int key = 0;
	Point2d playerXY = Point2d(0,0);
	double playerAngleRad = 0;
	int playerAngleDeg = 0;

	while (true) {
		timestamp start = std::chrono::high_resolution_clock::now();
		// int playerAngleDegree; DWORD data = {0};
		
		// playerAngleDegree = data;
		// std::printf("%d\n", playerAngleDegree);

		std::vector<angleDistance> angles;
		angleDistance angle;
		for(int i = 0; i < 360; i++){
			angle = *(new angleDistance(i, 9999));
			angles.push_back(angle);
		}
		//Mat original = Mat(40, 40, CV_8UC4, (Scalar)5);
		Mat original = getMat(hWND);
		Size s = original.size();
		int type = original.type();
		int width = s.width, height = s.height; //new: (1250, 805)
		Mat gray, edge;
		Mat empty(s, type);
		original.copyTo(gray);
		cvtColor(gray, gray, COLOR_BGR2GRAY);
		threshold(gray, gray, 200, 255, THRESH_BINARY);
		Canny(gray, edge, 10, 400, 3);
		std::vector<Vec4i> lines;
		HoughLinesP(edge, lines, 1, CV_PI / 180, 7, 5, 12);
		//key up
		keyboard.ki.dwFlags = KEYEVENTF_SCANCODE;
		SendInput(1, &keyboard, sizeof(INPUT));
		keyboard.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
		SendInput(1, &keyboard, sizeof(INPUT));	

		// circle(empty, Point(width*.5, height*.5), 85, Scalar(255, 0, 0), 1, 8, 0);
		// circle(empty, Point(width*.5, height*.5), 120, Scalar(255, 255, 255), 1, 8, 0);
		// circle(empty, Point(width*.5, height*.5), 1, Scalar(255, 0, 0), 1, 8, 0);



		std::vector<Wall> parallelWalls;
		checkAllDetections(lines, parallelWalls, angles, empty, width, height);

		setPlayerLocation(&playerXY, &playerAngleRad, &playerAngleDeg, hProcess, appBase, width, height);


		for(int i = 0; i < parallelWalls.size(); i++){
			parallelWalls[i].getAnglesCovered(width, height, angles);
		}


		std::vector<wallSegment> wallSegments;
		setWallSegments(wallSegments, angles, empty, playerAngleRad, width, height); /////////////////////////// <--- fix

		for(int i = 0; i < wallSegments.size(); i++){
			ellipse(empty, Point(width/2, height/2), Size(wallSegments[i].distToCenter, wallSegments[i].distToCenter), 0, -wallSegments[i].start, -wallSegments[i].end, Scalar(0,0,255));
		}

		String dir;
		double bestAngle = 0;
		if(wallSegments.size() > 0){
			chooseBestAngle(wallSegments, playerAngleDeg, &bestAngle);
		}

		compareAngles(bestAngle, playerAngleRad, &dir);
		if(dir == "None"){ keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_DOWN),0);
		}else if(dir == "Left") { keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_LEFT),0);
		}else if(dir == "Right"){ keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_RIGHT),0);}
		
		//key down
		keyboard.ki.dwFlags = KEYEVENTF_SCANCODE;
		SendInput(1, &keyboard, sizeof(INPUT));



		float testAngle;
		success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(appBase + Offsets::otherAngle2), &testAngle, sizeof(DWORD), NULL);
		assert(success);
		line(empty, Point(width/2, height/2), Point(300*cos((double)(testAngle * π/180)) + width/2, 300*sin((double)-(testAngle * π/180)) + height/2), Scalar(0, 255, 0), 1, LINE_AA);
		// success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(appBase + Offsets::otherAngle2), &testAngle, sizeof(DWORD), NULL);
		// assert(success);
		// line(empty, Point(width/2, height/2), Point(300*cos((double)(testAngle * π/180)) + width/2, 300*sin((double)-(testAngle * π/180)) + height/2), Scalar(0, 0, 255), 1, LINE_AA);

		
		circle(empty, playerXY, 4, Scalar(255, 255, 255), 6, 8, 0);
		// line(empty, Point(width/2, height/2), Point(300*cos(playerAngleRad) + width/2, 300*sin(-playerAngleRad) + height/2), Scalar(0, 255, 255), 1, LINE_AA);
		// line(empty, Point(width/2, height/2), Point(300*cos(bestAngle*(π/180)) + width/2, 300*sin(-bestAngle*(π/180)) + height/2), Scalar(0, 160, 255), 1, LINE_AA);
		// putText(empty, "Current: " + std::to_string((int)playerAngleRad*(180/π)), Point(10,70), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		// putText(empty, "Desired: " + std::to_string((int)bestAngle), Point(10,110), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		putText(empty, dir, Point(10,150), FONT_HERSHEY_SIMPLEX, 1, Scalar(255,0,255));

		timestamp end = std::chrono::high_resolution_clock::now();
		int fps = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		if (fps > 0) {
			putText(empty, "FPS: " + std::to_string(1000/fps), Point(10,30), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		}

		imshow("output", empty);
		key = waitKey(1);
		if(key == 27) break; // close on ESC
	}

	return 0;
}