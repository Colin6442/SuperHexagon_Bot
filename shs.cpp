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
#include "functions.h"


constexpr auto π = 3.14159265;

using namespace cv;

typedef std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;

struct Offsets{
	enum : DWORD {
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
};


int main() {
	// Get Game Window Info
	LPCSTR window_title = (LPCSTR)"Super Hexagon";
	HWND hWND = FindWindowA(NULL, window_title);
	namedWindow("output", WINDOW_NORMAL);
	
	// Setup Memory Access
	DWORD processId = -1;
	GetWindowThreadProcessId(hWND, &processId);
	if (processId == -1){
		std::cout << "Open ProcessID Failed." << std::endl;
	}
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
	if (hProcess == NULL){
		std::cout << "Open Process Failed." << std::endl;
	}

	DWORD appBase = GetBaseAddress(processId, L"SuperHexagon.exe");

	DWORD basePointer = appBase + Offsets::BaseOffset;

	bool success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(basePointer), &appBase, sizeof(DWORD), NULL);
	assert(success);

	// Setup Keyboard Inputs
	INPUT keyboard;
	keyboard.type = INPUT_KEYBOARD;
	keyboard.ki.time = 0;
	keyboard.ki.wVk = 0;
	keyboard.ki.dwExtraInfo = 0;
	keyboard.ki.dwFlags = 0;
	keyboard.ki.wVk = 0;

	// Out of Loop Variables
	int count = 0;
	int esc = 0;
	Point2d playerXY = Point2d(0,0);
	double playerAngleRad = 0;
	int playerAngleDeg = 0;
	String dir = "None";

	while (true) {

		// Get time for FPS
		timestamp start = std::chrono::high_resolution_clock::now();

		// Initialize angles
		std::vector<angleDistance> angles;
		angleDistance angle;
		for(int i = 0; i < 360; i++){
			angle = *(new angleDistance(i, 9999));
			angles.push_back(angle);
		}

		// Process Image from Game
		Mat original = getMat(hWND);

		// Read memory first, then compute
		float worldAngle;
		success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(appBase + Offsets::worldAngle), &worldAngle, sizeof(DWORD), NULL);
			assert(success);

		bool success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(appBase + Offsets::PlayerAngle), &playerAngleDeg, sizeof(DWORD), NULL);
			assert(success);

		float offset;
		success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(appBase + Offsets::PlayerAngleOffset), &offset, sizeof(DWORD), NULL);
			assert(success);
		
		// Continuue processing image
		Size s = original.size();
		int type = original.type();
		int width = s.width, height = s.height; //big: (1250, 805)   small: (768, 480)
		Mat gray, edge;
		Mat outputWindow(s, type);
		Mat shown(s, type);
		original.copyTo(gray);
		cvtColor(gray, gray, COLOR_BGR2GRAY);
		threshold(gray, gray, 200, 255, THRESH_BINARY);
		Canny(gray, edge, 10, 400, 3);
		std::vector<Vec4i> lines;
		HoughLinesP(edge, lines, 1, CV_PI / 180, 7, 5, 12);

		// Unpress Key
		// keyboard.ki.dwFlags = KEYEVENTF_SCANCODE;
		// SendInput(1, &keyboard, sizeof(INPUT));
		// keyboard.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
		// SendInput(1, &keyboard, sizeof(INPUT));	

		// Run Detection Functions
		std::vector<Wall> parallelWalls;
		checkAllDetections(lines, parallelWalls, angles, outputWindow, width, height);
		setPlayerLocation(&playerXY, &playerAngleRad, &playerAngleDeg, &offset, dir, hProcess, appBase, width, height);
		for(int i = 0; i < parallelWalls.size(); i++){
			parallelWalls[i].getAnglesCovered(width, height, angles);
		}

		// Setup Walls and Add to Screen
		int sectionAngles [6];
		for(int i = (int(worldAngle+30)%60), j = 0; j < 6; i+=60, j++){
			sectionAngles[j] = i;
		}

		std::vector<wallSegment> wallSegments;
		setWallSegments(wallSegments, angles, sectionAngles, playerAngleRad, width, height);
		for(int i = 0; i < wallSegments.size(); i++){
			ellipse(outputWindow, Point(width/2, height/2), Size(wallSegments[i].distToCenter, wallSegments[i].distToCenter), 0, -wallSegments[i].start, -wallSegments[i].end, Scalar(0,255,0));
		}
		for(int i = 0; i < 6; i++){
			putText(outputWindow, std::to_string((int)wallSegments[i].distToCenter), Point(200*cos((double)((sectionAngles[i] + 30) * π/180)) + width/2, 100*sin((double)-((sectionAngles[i] + 30) * π/180)) + height/2), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		}
		// Select Best Angle to Use
		double bestAngle = 0;
		if(wallSegments.size() > 0){
			chooseBestAngle(wallSegments, playerAngleDeg, &bestAngle);
		}

		// Set Key Press
		compareAngles(/*bestAngle*/worldAngle, playerAngleRad, &dir);
		// dir = "Left";
		if(dir == "None"){ keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_DOWN),0);
		}else if(dir == "Left") { keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_LEFT),0);
		}else if(dir == "Right"){ keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_RIGHT),0);}
		
		// Key Down
		// keyboard.ki.dwFlags = KEYEVENTF_SCANCODE;
		// SendInput(1, &keyboard, sizeof(INPUT));
	
		line(outputWindow, Point(width/2, height/2), Point(300*cos((double)(worldAngle * π/180)) + width/2, 300*sin((double)-(worldAngle * π/180)) + height/2), Scalar(0, 0, 255), 1, LINE_AA);
		for(int i = 0; i < 6; i++){
			line(outputWindow, Point(width/2, height/2), Point(300*cos((double)(sectionAngles[i] * π/180)) + width/2, 300*sin((double)-(sectionAngles[i] * π/180)) + height/2), Scalar(255, 0, 0), 1, LINE_AA);
		}

		// Add player/text to outputWindow
		circle(outputWindow, playerXY, 4, Scalar(255, 255, 255), 6, 8, 0);
		putText(outputWindow, dir, Point(10,150), FONT_HERSHEY_SIMPLEX, 1, Scalar(255,0,255));
		timestamp end = std::chrono::high_resolution_clock::now();
		int fps = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		if (fps > 0) {
			putText(outputWindow, "FPS: " + std::to_string(1000/fps), Point(10,30), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		}

		// Show Output, Close if ESC Pressed
		addWeighted(outputWindow, 0.8, original, 0.2, 1, shown);
		imshow("output", shown);
		esc = waitKey(1);
		if(esc == 27) break; // close on ESC
	}
	while(true){}
	return 0;
}