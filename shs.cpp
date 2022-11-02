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

struct PlayerDetectionCircle {
	std::vector<Point> containedPoints;
	bool hasPoints;
	int xPos;
	int yPos;
	int radius;
	int avgX;
	int avgY;
	double score = 100;

	PlayerDetectionCircle() {
		xPos = 0;
		yPos = 0;
		radius = 0;
		hasPoints = false;
	}

	PlayerDetectionCircle(int x, int y, int radIn){
		xPos = x;
		yPos = y;
		radius = radIn;
		hasPoints = false;
	}

	void addPoint(Point newPoint){
		containedPoints.push_back(newPoint);
		hasPoints = true;
	}

	Point calcAvgPos(){
		int currAvgX = 0;
		int currAvgY = 0;
		for(int i = 0; i < containedPoints.size(); i++){
			currAvgX += containedPoints[i].x;
			currAvgY += containedPoints[i].y;
		}
		if (containedPoints.size() > 0) {
			currAvgX /= containedPoints.size(); // size somehow 0?
			currAvgY /= containedPoints.size();
			avgX = currAvgX;
			avgY = currAvgY;
		}

		return(Point(currAvgX, currAvgY));
	}

	double calcClusterScore(){
		if (containedPoints.size() > 1) {
			int dist = 0;
			for (int i = 0; i < containedPoints.size(); i++) {
				for (int j = i; j < containedPoints.size(); j++) {
					if (i != j) {
						dist += sqrt(pow(containedPoints[i].x - containedPoints[j].x, 2));
						dist += sqrt(pow(containedPoints[i].y - containedPoints[j].y, 2));
					}
				}
			}
			score = (double)dist / pow(containedPoints.size(), containedPoints.size());
			return score;
		}
		return 0;
	}

};

bool detectPlayer(std::vector<PlayerDetectionCircle>& allPossible, Mat& empty, Point pos, int width, int height) {
	double fromCenter = sqrt(pow(pos.x - width*.5, 2) + pow(pos.y - height*.5, 2)); // distance from center of game/screen
	if (fromCenter > 85 && fromCenter < 120){
		for(int i = 0; i < allPossible.size(); i++){
			// circle(empty, Point(allPossible[i].xPos, allPossible[i].yPos), 41, Scalar(0, 255, 0), 4, 8, 0);
			fromCenter = sqrt(pow(pos.x - allPossible[i].xPos, 2) + pow(pos.y - allPossible[i].yPos, 2)); // distance from center of PlayerDetectionCircle
			if (fromCenter < 41){
				allPossible[i].addPoint(pos);
				// circle(empty, pos, 4, Scalar(0, 160, 255), 4, 8, 0);
			}
		}
		return true;
	}
	return false;
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

void createPlayerMSPs(std::vector<PlayerDetectionCircle>& allPossible, int width, int height){
		PlayerDetectionCircle input = *(new PlayerDetectionCircle(width * .5, height * .5 - 105, 41));
		allPossible.push_back(input);
		input = *(new PlayerDetectionCircle(width*.5 + 105*sqrt(2)/2, height*.5 - 105*sqrt(2)/2, 41));
		allPossible.push_back(input);
		input = *(new PlayerDetectionCircle(width*.5 + 105, height*.5, 41));
		allPossible.push_back(input);
		input = *(new PlayerDetectionCircle(width*.5 + 105*sqrt(2)/2, height*.5 + 105*sqrt(2)/2, 41));
		allPossible.push_back(input);
		input = *(new PlayerDetectionCircle(width*.5, height*.5 + 105, 41));
		allPossible.push_back(input);
		input = *(new PlayerDetectionCircle(width*.5 - 105*sqrt(2)/2, height*.5 + 105*sqrt(2)/2, 41));
		allPossible.push_back(input);
		input = *(new PlayerDetectionCircle(width*.5 - 105, height*.5, 41));
		allPossible.push_back(input);
		input = *(new PlayerDetectionCircle(width*.5 - 105*sqrt(2)/2, height*.5 - 105*sqrt(2)/2, 41));
		allPossible.push_back(input);
}

void checkAllDetections(std::vector<Vec4i> lines, std::vector<Wall>& parallelWalls, std::vector<angleDistance>& angles, bool *pointDetected, Mat& empty, std::vector<PlayerDetectionCircle>& allPossible, int width, int height){
		double length;
		for(size_t i = 0; i < lines.size(); i++) {
			Vec4i l = lines[i];
			length = sqrt(pow((double)(l[0] - l[2]), 2) + pow((double)(l[1] - l[3]), 2));
			Point pos = Point(abs(l[0] + l[2]) / 2, abs(l[1] + l[3]) / 2);
			// std::printf("in: %d, %d    out: %d, %d\n", x, y, pos.x, pos.y);
			// line(empty, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0, 0, 255), 1, LINE_AA);
			if(length < 20){
				if(detectPlayer(allPossible, empty, pos, width, height)){
					*pointDetected = true;
				}
			}
			if(pos.y > 70 && sqrt(pow(pos.x - width*.5, 2) + pow(pos.y - height*.5, 2)) > 120){
				detectWalls(parallelWalls, empty, angles, Point(l[0], l[1]), Point(l[2], l[3]), pos, width, height);
				// circle(empty, pos, 4, Scalar(255, 255, 0), 6, 8, 0);
			}
		}
}

void setPlayerLocation(std::vector<PlayerDetectionCircle>& allPossible, double* currentPlayerLocX, double* currentPlayerLocY, float alpha, double* playerAngle, int width, int height) {
	double lowestScore = 9999999;
	int lowestIndex = 0;
	Point playerLoc;
	for(int i = 0; i < allPossible.size(); i++) {
		if(allPossible[i].hasPoints && allPossible[i].calcClusterScore() < lowestScore){
			lowestScore = allPossible[i].score;
			lowestIndex = i;
		}
	}
	if(allPossible.size() > 0){
		playerLoc = allPossible[lowestIndex].calcAvgPos();
		*currentPlayerLocX = alpha * (playerLoc.x) + (1 - alpha) * *currentPlayerLocX;
		*currentPlayerLocY = alpha * (playerLoc.y) + (1 - alpha) * *currentPlayerLocY;
	}

	*playerAngle = atan(-(*currentPlayerLocY - (height / 2)) / (*currentPlayerLocX - (width / 2)));
	if (*currentPlayerLocX > width / 2 && *currentPlayerLocY > height / 2) { // +x, +y
		*playerAngle += 2 * π;
	}else if (*currentPlayerLocX < width / 2) {
		*playerAngle += π;
	}
}

void movePlayer(INPUT keyboard, String dir){
	
	if(dir == "Left"){keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_LEFT),0);}
	if(dir == "Right"){keyboard.ki.wScan = MapVirtualKey(LOBYTE(VK_RIGHT),0);}
	
	keyboard.ki.dwFlags = KEYEVENTF_SCANCODE;

	SendInput(1, &keyboard, sizeof(INPUT));
	
	Sleep(1);
	
	keyboard.ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &keyboard, sizeof(INPUT));
}

int main() {
	LPCTSTR window_title = (LPCTSTR)"Super Hexagon";
	HWND hWND = FindWindow(NULL, window_title);
	namedWindow("output", WINDOW_NORMAL);
	
	INPUT keyboard;
	keyboard.type = INPUT_KEYBOARD;
	keyboard.ki.time = 0;
	keyboard.ki.wVk = 0;
	keyboard.ki.dwExtraInfo = 0;
	keyboard.ki.dwFlags = 0;
	keyboard.ki.wVk = 0;

	int key = 0;
	double currentPlayerLocX = 0;
	double currentPlayerLocY = 0;
	double playerAngle = 0;
	bool runbot = true;

	while (runbot) {
		timestamp start = std::chrono::high_resolution_clock::now();
		
		keyboard.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &keyboard, sizeof(INPUT));
		
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


		std::vector<PlayerDetectionCircle> allPossible;
		createPlayerMSPs(allPossible, width, height);

		// circle(empty, Point(width*.5, height*.5), 85, Scalar(255, 0, 0), 1, 8, 0);
		// circle(empty, Point(width*.5, height*.5), 120, Scalar(255, 255, 255), 1, 8, 0);
		// circle(empty, Point(width*.5, height*.5), 1, Scalar(255, 0, 0), 1, 8, 0);

		
		bool pointDetected = false;
		std::vector<Wall> parallelWalls;

		checkAllDetections(lines, parallelWalls, angles ,&pointDetected, empty, allPossible, width, height);

		float alpha = 0.5;
		if(pointDetected){
			setPlayerLocation(allPossible, &currentPlayerLocX, &currentPlayerLocY, alpha, &playerAngle, width, height);
		}

		for(int i = 0; i < parallelWalls.size(); i++){
			parallelWalls[i].getAnglesCovered(width, height, angles);
		}

		double furthest = 0;
		int angleIndex;
		for(int i = 0; i < 360; i++){
			if(angles[i].distances[0] > furthest){
				furthest = angles[i].distances[0];
				angleIndex = i;
			}
			
			// if(angles[i].distances[0] < 9000){
			// 	circle(empty, Point(angles[i].distances[0]*cos(angles[i].radian) + width/2, angles[i].distances[0]*sin(-angles[i].radian) + height/2), 1, Scalar(0,0,255));
			// }else{
			// 	circle(empty, Point(angles[i].distances[0]*cos(angles[i].radian) + width/2, angles[i].distances[0]*sin(-angles[i].radian) + height/2), 1, Scalar(255,255,255));
			// }
		}

		if(angles[int(playerAngle*(180/π))].distances[0] > 9000){
			angleIndex = int(playerAngle*(180/π));
		}

		String dir;

		// C: playerAngle*(180/π)
		// D: angleIndex
		double CD = abs(playerAngle*(180/π) - angleIndex);
		double CDp360 = playerAngle*(180/π) - angleIndex + 360;
		double CDm360 = playerAngle*(180/π) - angleIndex + 360;

		if(CD < CDp360 && CD < CDm360){
			if(playerAngle*(180/π) - angleIndex > 0){ dir = "Right";
			}else{ 								      dir = "Left"; }
		}else if(CDp360 < CD && CDp360 < CDm360){
			dir = "Right";
		}else if(CDm360 < CD && CDm360 < CDp360){
			dir = "Left";
		}
		
		if(abs(int(playerAngle*(180/π)) - angleIndex) > 1){
			movePlayer(keyboard, dir);
		}





		// String text = std::to_string(playerAngle) + " = arctan(" + std::to_string(-(currentPlayerLocY-(height/2)) ) + " / " + std::to_string((currentPlayerLocX-(width/2))) + ")";
		// String calc = "cos(" + std::to_string(playerAngle) + ") = " + std::to_string(cos(playerAngle)) + " \t sin(" + std::to_string(playerAngle) + ") = " + std::to_string(sin(playerAngle));		
		// circle(empty, Point(currentPlayerLocX, currentPlayerLocY), 4, Scalar(255, 0, 0), 6, 8, 0);
		circle(empty, Point(120*cos(playerAngle) + width/2, 120*sin(-playerAngle) + height/2), 4, Scalar(255, 0, 0), 6);
		// putText(empty, text, Point(10,30), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));

		timestamp end = std::chrono::high_resolution_clock::now();
		int fps = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		if (fps > 0) {
			putText(empty, "FPS: " + std::to_string(1000/fps), Point(10,30), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		}

		line(empty, Point(width/2, height/2), Point(300*cos(playerAngle) + width/2, 300*sin(-playerAngle) + height/2), Scalar(0, 255, 255), 1, LINE_AA);
		line(empty, Point(width/2, height/2), Point(300*cos(angleIndex*(π/180)) + width/2, 300*sin(-angleIndex*(π/180)) + height/2), Scalar(0, 160, 255), 1, LINE_AA);
		putText(empty, "Current: " + std::to_string(playerAngle*(180/π)), Point(10,70), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		putText(empty, "Desired: " + std::to_string(angleIndex), Point(10,110), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		putText(empty, dir, Point(10,150), FONT_HERSHEY_SIMPLEX, 1, Scalar(255,0,255));

		// putText(empty, "Score: " + std::to_string(lowestScore), Point(10,70), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		imshow("output", empty);
		key = waitKey(1);
	}

}
