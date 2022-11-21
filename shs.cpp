#include <cstdio>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>

#include <iostream>
#include <string.h>
#include <chrono>
#include <math.h> 

#include <vector>
#include <windows.h>
#include <Windows.h>

#define π 3.14159265

using namespace cv;
typedef std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;

struct Offsets{
	enum : DWORD {
		BasePointer = 0x750000 + 0x15E8EC,  // fix, not static base

		PlayerAngle = 0x1E0,
		MouseDownLeft = 0x38,
		MouseDownRight = 0x3C,
		otherAngle1 = 0x1D8,
		otherAngle2 = 0x1DC,
		otherAngle3 = 0x1AC,
	};
};

struct circularList{
	int size;
	int current;

	circularList(int current, int size){
		this->size = size;
		this->current = current;
	}

	circularList& operator+=(const int& num){
		int fixed = num%size;
		if(current + fixed <= size-1 && current + fixed >= 0){
			current += fixed;
		}else if(current + fixed > size-1){
			current = current + fixed - size;
		}else if(current + fixed < 0){
			current = size + current + fixed;
		}

		return *this;
	}

	circularList& operator-=(const int& num){
		int fixed = num%size;
		if(current - fixed <= size-1 && current - fixed >= 0){
			current -= fixed;
		}else if(current - fixed > size-1){
			current = current - fixed - size;
		}else if(current - fixed < 0){
			current = size + current - fixed;
		}
		
		return *this;
	}
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

struct angleDistance{
	int degree;
	double radian;
	std::vector<double> distances;

	angleDistance(){}
	
	angleDistance(int ang, double dist){
		degree = ang;
		radian = ang*(π/180);
		distances.push_back(dist);
	}

	void addWall(double dist){
		distances.push_back(dist);
		std::sort(distances.begin(), distances.end());
	}

	double getClosestWall(){
		return distances[0];
	}

};

struct Wall{
	double distToCenter;
	double slope;
	Point point1;
	Point point2;
	Point middle;
	double length;

	Wall(Point pt1, Point pt2, double slopeIn, double toCenter, Point mid){
		point1 = pt1;
		point2 = pt2;
		length = sqrt(pow(pt1.x - pt2.x, 2) + pow(pt1.y - pt2.y, 2));
		slope = slopeIn;
		distToCenter = toCenter;
		middle = mid;
	}

	void getAnglesCovered(int width, int height, std::vector<angleDistance>& angles){
		//Calc angle for point1
		double point1Y = (point1.y - (height / 2)), point1X = (point1.x - (width / 2));
		double point1Radians = atan(-point1Y/point1X);
		if (point1.x > width / 2 && point1.y > height / 2) {point1Radians += 2*π;
		}else if (point1.x < width / 2) {point1Radians += π;}
		double point1Angle = point1Radians * (180/π);

		//Calc angle for point2
		double point2Y = (point2.y - (height / 2)), point2X = (point2.x - (width / 2));
		double point2Radians = atan(-point2Y/point2X);
		if (point2.x > width / 2 && point2.y > height / 2) {point2Radians += 2*π;
		}else if (point2.x < width / 2) {point2Radians += π;}
		double point2Angle = point2Radians * (180/π);

		int j = 1;
		if(point1Angle > point2Angle && sqrt(pow(point1Angle - point2Angle, 2)) < 180){
			for(int i = point2Angle; i < point1Angle; i++){
				angles[i].addWall(distToCenter);
				if(i > 0 && i < 359){
					angles[i + 1].addWall(distToCenter);
					angles[i - 1].addWall(distToCenter);
				}
			}
		}else if(point1Angle < point2Angle && sqrt(pow(point1Angle - point2Angle, 2)) < 180){
			for(int i = point1Angle; i < point2Angle; i++){
				angles[i].addWall(distToCenter);
				if(i > 0 && i < 359){
					angles[i + 1].addWall(distToCenter);
					angles[i - 1].addWall(distToCenter);
				}
			}
		}else if(point1Angle > point2Angle && sqrt(pow(point1Angle - point2Angle, 2)) > 180){
			int j = 361;
			for(int i = point1Angle; i < j; i++){
				if(i == 360){
					i = 0;
					j = point2Angle;
				}
				angles[i].addWall(distToCenter);
				if(i > 0 && i < 359){
					angles[i + 1].addWall(distToCenter);
					angles[i - 1].addWall(distToCenter);
				}
			}
		}else if(point1Angle < point2Angle && sqrt(pow(point1Angle - point2Angle, 2)) > 180){
			int j = 361;
			for(int i = point2Angle; i < j; i++){
				if(i == 360){
					i = 0;
					j = point1Angle;
				}
				angles[i].addWall(distToCenter);
				if(i > 0 && i < 359){
					angles[i + 1].addWall(distToCenter);
					angles[i - 1].addWall(distToCenter);
				}
			}
		}
	}


};

struct wallSegment{
	int start;
	int middle;
	int end;
	double distToCenter;
	double distFromPlayer;
	String dirFromPlayer;

	wallSegment(){}

	wallSegment(int startIn, int endIn, double playerAngleRad, double toCenter){
		start = startIn;
		end = endIn;
		middle = (end + start)/2;
		distToCenter = toCenter;
		double middleAngle = (π/180)*((endIn + startIn)/2);
		distFromPlayer = compareAngles(middleAngle, playerAngleRad, &dirFromPlayer);
	}
};



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

bool circularListCheck(int at, int check, int thresh, int size){
	circularList begining(at, size);
	circularList ending(at, size);
	begining -= thresh;
	ending += thresh;
	while(begining.current != ending.current){
		if(begining.current == check){
			return true;
		}
		begining.current += 1;
	}
	return false;
}

void setPlayerLocation(Point2d* playerXY, double* playerAngleRad, int* playerAngleDeg, HANDLE hProcess, DWORD appBase, int width, int height) {
	bool success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(appBase + Offsets::PlayerAngle), playerAngleDeg, sizeof(DWORD), NULL);
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
		success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(appBase + Offsets::otherAngle3), &testAngle, sizeof(DWORD), NULL);
		assert(success);
		line(empty, Point(width/2, height/2), Point(300*cos((double)(testAngle * π/180)) + width/2, 300*sin((double)-(testAngle * π/180)) + height/2), Scalar(0, 255, 0), 1, LINE_AA);
		// success = ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(appBase + Offsets::otherAngle2), &testAngle, sizeof(DWORD), NULL);
		// assert(success);
		// line(empty, Point(width/2, height/2), Point(300*cos((double)(testAngle * π/180)) + width/2, 300*sin((double)-(testAngle * π/180)) + height/2), Scalar(0, 0, 255), 1, LINE_AA);

		
		circle(empty, playerXY, 4, Scalar(255, 255, 255), 6, 8, 0);
		line(empty, Point(width/2, height/2), Point(300*cos(playerAngleRad) + width/2, 300*sin(-playerAngleRad) + height/2), Scalar(0, 255, 255), 1, LINE_AA);
		line(empty, Point(width/2, height/2), Point(300*cos(bestAngle*(π/180)) + width/2, 300*sin(-bestAngle*(π/180)) + height/2), Scalar(0, 160, 255), 1, LINE_AA);
		putText(empty, "Current: " + std::to_string((int)playerAngleRad*(180/π)), Point(10,70), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		putText(empty, "Desired: " + std::to_string((int)bestAngle), Point(10,110), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
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