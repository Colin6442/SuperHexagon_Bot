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

#include "variables.h"
#include "wallClasses.h"
#include "circularList.h"
#include "functions.h"


constexpr auto π = 3.14159265;

using namespace cv;

typedef std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;

int main() {

	// Turn keyboard inputs off/on
	bool movement = true;

	// Variable Structure
	Variables vars;

	// Get Game Window Info
	LPCSTR window_title = (LPCSTR)"Super Hexagon";
	HWND hWND = FindWindowA(NULL, window_title);
	// namedWindow("output", WINDOW_NORMAL); //create window to show outputs

	// Setup Memory Access
	DWORD processId = -1;
	GetWindowThreadProcessId(hWND, &processId);
	if (processId == -1){
		std::cout << "Open ProcessID Failed." << std::endl;
	}
	vars.hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
	if (vars.hProcess == NULL){
		std::cout << "Open Process Failed." << std::endl;
	}
	

	vars.appBase = GetBaseAddress(processId, L"SuperHexagon.exe");

	DWORD basePointer = vars.appBase + vars.BaseOffset;

	bool success = ReadProcessMemory(vars.hProcess, reinterpret_cast<LPCVOID>(basePointer), &vars.appBase, sizeof(DWORD), NULL);
	assert(success);

	// Setup Keyboard Inputs
	int esc = 0;
	INPUT keyboard;
	keyboard.type = INPUT_KEYBOARD;
	keyboard.ki.time = 0;
	keyboard.ki.wVk = 0;
	keyboard.ki.dwExtraInfo = 0;
	keyboard.ki.dwFlags = 0;
	keyboard.ki.wVk = 0;
	
	// Process Image from Game
	Mat original = getMat(hWND);
	Size s = original.size();
	//big: (1250, 805)   small: (768, 480)
	vars.width = s.width;
	vars.height = s.height; 


	while (true) {
		// Start timer for FPS
		timestamp start = std::chrono::high_resolution_clock::now();

		// Initialize angles
		// std::vector<angleDistance> angles;
		for(int i = 0; i < 360; i++){
			angleDistance angle = *(new angleDistance(i, 450));
			vars.angles.push_back(angle);
		}


		// Read memory first, then compute
		float worldAngle;
		success = ReadProcessMemory(vars.hProcess, reinterpret_cast<LPCVOID>(vars.appBase + vars.worldAngle), &worldAngle, sizeof(DWORD), NULL);
			assert(success);

		success = ReadProcessMemory(vars.hProcess, reinterpret_cast<LPCVOID>(vars.appBase + vars.PlayerAngle), &vars.playerAngleDeg, sizeof(DWORD), NULL);
			assert(success);

		success = ReadProcessMemory(vars.hProcess, reinterpret_cast<LPCVOID>(vars.appBase + vars.PlayerAngleOffset), &vars.playerOffset, sizeof(DWORD), NULL);
			assert(success);

		success = ReadProcessMemory(vars.hProcess, reinterpret_cast<LPCVOID>(vars.appBase + vars.relativeAngle), &vars.relativeAngles, sizeof(DWORD), NULL);
			assert(success);
		
		// Continuue processing image
		original = getMat(hWND);
		s = original.size();
		Mat gray, edge;
		Mat outputWindow(s, original.type());
		Mat shown(s, original.type());
		original.copyTo(gray);
		cvtColor(gray, gray, COLOR_BGR2GRAY);
		threshold(gray, gray, 200, 255, THRESH_BINARY);
		Canny(gray, edge, 10, 400, 3);
		std::vector<Vec4i> lines;
		HoughLinesP(edge, lines, 1, CV_PI / 180, 7, 5, 12);

		// Run Detection Functions
		checkAllDetections(lines, outputWindow, &vars);
		setPlayerLocation(&vars);
		for(int i = 0; i < vars.parallelWalls.size(); i++){
			vars.parallelWalls[i].getAnglesCovered(vars.width, vars.height, vars.angles);
		}
		
		// Unpress Key
		if(movement){
			keyboard.ki.dwFlags = KEYEVENTF_SCANCODE;
			SendInput(1, &keyboard, sizeof(INPUT));
			keyboard.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
			SendInput(1, &keyboard, sizeof(INPUT));	
		}

		// Setup Walls and Add to Screen
		for(int i = (int(worldAngle+30)%60), j = 0; j < 6; i+=60, j++){
			vars.sectionAngles[j] = i;
		}

		setWallSegments(&vars);
		
		// Compute best slot to use
		bestDirection(&vars, outputWindow);

		for(int i = 0; i < vars.wallSegments.size(); i++){
			// ellipse(outputWindow, Point(vars.width/2, vars.height/2), Size(vars.wallSegments[i].distToCenter, vars.wallSegments[i].distToCenter), 0, -vars.wallSegments[i].start, -vars.wallSegments[i].end, Scalar(0,255,0));
			
			if (vars.playerAngleDeg > vars.wallSegments[i].start && vars.playerAngleDeg < vars.wallSegments[i].end){
				if(i == 6){
					vars.playerSlot = 5;
				}else{
					vars.playerSlot = i;
				}
				// ellipse(outputWindow, Point(vars.width/2, vars.height/2), Size(50, 50), 0, -vars.wallSegments[i].start, -vars.wallSegments[i].end, Scalar(0,0,255));
			}
			
			if(vars.best_slot == i){
				// ellipse(outputWindow, Point(vars.width/2, vars.height/2), Size(60, 60), 0, -vars.wallSegments[i].start, -vars.wallSegments[i].end, Scalar(255,0,0));
			}
		}

		// compareAngles()

		// display debug stuff
		
		/*
		for(int i = 0; i < 6; i++){
			putText(outputWindow, std::to_string((int)vars.wallSegments[i].distToCenter), Point(200*cos((double)((vars.sectionAngles[i] + 30) * π/180)) + vars.width/2, 200*sin((double)-((vars.sectionAngles[i] + 30) * π/180)) + vars.height/2), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
			putText(outputWindow, std::to_string((int)vars.wallSegments[i].angleFromPlayer), Point(100*cos((double)((vars.sectionAngles[i] + 30) * π/180)) + vars.width/2, 100*sin((double)-((vars.sectionAngles[i] + 30) * π/180)) + vars.height/2), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(255,255,255));
			// putText(outputWindow, std::to_string(i), Point(150*cos((double)((vars.sectionAngles[i] + 30) * π/180)) + vars.width/2, 150*sin((double)-((vars.sectionAngles[i] + 30) * π/180)) + vars.height/2), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0,160,255));
			putText(outputWindow, std::to_string(vars.wallSegments[i].score), Point(150*cos((double)((vars.sectionAngles[i] + 30) * π/180)) + vars.width/2, 150*sin((double)-((vars.sectionAngles[i] + 30) * π/180)) + vars.height/2), FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0,160,255));

		}
		*/
		
		
		// Set Key Press
		if(vars.dir == "None"){         keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_DOWN),0);
		}else if(vars.dir == "Left") {  keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_LEFT),0);
		}else if(vars.dir == "Right"){  keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_RIGHT),0);}
		
		// Key Down
		if(movement){
			keyboard.ki.dwFlags = KEYEVENTF_SCANCODE;
			SendInput(1, &keyboard, sizeof(INPUT));
		}

		// Add player/text to outputWindow
		/*
		circle(outputWindow, vars.playerXY, 4, Scalar(255, 255, 255), 6, 8, 0);
		putText(outputWindow, vars.dir, Point(10,150), FONT_HERSHEY_SIMPLEX, 1, Scalar(255,0,255));
		putText(outputWindow, std::to_string(vars.playerSlot), Point(10,200), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		*/
		vars.resetInLoopVars();
		
		/*
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
		*/
	}
	return 0;
}