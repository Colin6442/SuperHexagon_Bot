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

#include <vector>
#include <windows.h>
#include <Windows.h>

using namespace cv;

struct MeanShiftPoint {
	std::vector<Point> containedPoints;
	bool hasPoints;
	int xPos;
	int yPos;
	int radius;
	int avgX;
	int avgY;
	double score = 100;

	MeanShiftPoint() {
		xPos = 0;
		yPos = 0;
		radius = 0;
		hasPoints = false;
	}

	MeanShiftPoint(int x, int y, int radIn){
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
	bi.biSizeImage = 0; //because no compression
	bi.biXPelsPerMeter = 1; //we
	bi.biYPelsPerMeter = 2; //we
	bi.biClrUsed = 3; //we ^^
	bi.biClrImportant = 4; //still we

	Mat mat = Mat(height, width, CV_8UC4); // 8 bit unsigned ints 4 Channels -> RGBA

	//transform data and store into mat.data
	GetDIBits(memoryDeviceContext, bitmap, 0, height, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

	//clean up!
	DeleteObject(bitmap);
	DeleteDC(memoryDeviceContext); //delete not release!
	ReleaseDC(hWND, deviceContext);

	return mat;
}

bool detectPlayer(std::vector<MeanShiftPoint>& allPossible, Mat& empty, Point pos, int width, int height) {
	double fromCenter = sqrt(pow(pos.x - width*.5, 2) + pow(pos.y - height*.5, 2)); // distance from center of game/screen
	if (fromCenter > 85 && fromCenter < 120){
		for(int i = 0; i < allPossible.size(); i++){
			fromCenter = sqrt(pow(pos.x - allPossible[i].xPos, 2) + pow(pos.y - allPossible[i].yPos, 2)); // distance from center of MeanShiftPoint
			if (fromCenter < 41){
				allPossible[i].addPoint(pos);
				//circle(empty, pos, 4, Scalar(0, 255, 0), 4, 8, 0);
			}
		}
		return true;
	}
	return false;
}

int main() {
	LPCTSTR window_title = (LPCTSTR)"Super Hexagon";
	HWND hWND = FindWindow(NULL, window_title);
	namedWindow("output", WINDOW_NORMAL);
	int key = 0;
	int currentPlayerLocX = 0;
	int currentPlayerLocY = 0;
	bool runbot = true;
	while (runbot) {
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
		HoughLinesP(edge, lines, 1, CV_PI / 180, 7, 5, 10);

		std::vector<MeanShiftPoint> allPossible;
		MeanShiftPoint input = *(new MeanShiftPoint(width * .5, height * .5 - 105, 41));
		allPossible.push_back(input);
		input = *(new MeanShiftPoint(width*.5 + 105*sqrt(2)/2, height*.5 - 105*sqrt(2)/2, 41));
		allPossible.push_back(input);
		input = *(new MeanShiftPoint(width*.5 + 105, height*.5, 41));
		allPossible.push_back(input);
		input = *(new MeanShiftPoint(width*.5 + 105*sqrt(2)/2, height*.5 + 105*sqrt(2)/2, 41));
		allPossible.push_back(input);
		input = *(new MeanShiftPoint(width*.5, height*.5 + 105, 41));
		allPossible.push_back(input);
		input = *(new MeanShiftPoint(width*.5 - 105*sqrt(2)/2, height*.5 + 105*sqrt(2)/2, 41));
		allPossible.push_back(input);
		input = *(new MeanShiftPoint(width*.5 - 105, height*.5, 41));
		allPossible.push_back(input);
		input = *(new MeanShiftPoint(width*.5 - 105*sqrt(2)/2, height*.5 - 105*sqrt(2)/2, 41));
		allPossible.push_back(input);

		// circle(empty, Point(width*.5, height*.5), 85, Scalar(255, 0, 0), 1, 8, 0);
		// circle(empty, Point(width*.5, height*.5), 120, Scalar(255, 0, 0), 1, 8, 0);
		// circle(empty, Point(width*.5, height*.5), 1, Scalar(255, 0, 0), 1, 8, 0);



		int length;
		bool pointDetected = false;
		for(size_t i = 0; i < lines.size(); i++) {
			Vec4i l = lines[i];
			length = sqrt(pow((double)(l[0] - l[2]), 2) + pow((double)(l[1] - l[3]), 2));
			Point pos = Point(abs(l[0] + l[2]) / 2, abs(l[1] + l[3]) / 2);
			// std::printf("in: %d, %d    out: %d, %d\n", x, y, pos.x, pos.y);
			circle(empty, pos, 4, Scalar(255, 255, 0), 6, 8, 0);
			line(empty, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0, 0, 255), 1, LINE_AA);
			if (length < 20){
				if(detectPlayer(allPossible, empty, pos, width, height)){
					pointDetected = true;
				}
			}
		}
			
		float alpha = 0.8;
		Point playerLoc;
		double lowestScore = 9999999;
		int lowestIndex = 0;
		if(pointDetected){
			for(int i = 0; i < allPossible.size(); i++) {
				if(allPossible[i].hasPoints && allPossible[i].calcClusterScore() < lowestScore){
					//circle(empty, Point(allPossible[i].xPos, allPossible[i].yPos), 41, Scalar(255, 0, 0), 6, 8, 0);
					lowestScore = allPossible[i].score;
					lowestIndex = i;
				}
			}
			//circle(empty, Point(allPossible[lowestIndex].xPos, allPossible[lowestIndex].yPos), 41, Scalar(0, 160, 255), 4, 8, 0);
			if(allPossible.size() > 0){
				playerLoc = allPossible[lowestIndex].calcAvgPos();
				currentPlayerLocX = alpha * (playerLoc.x) + (1 - alpha) * currentPlayerLocX;
				currentPlayerLocY = alpha * (playerLoc.y) + (1 - alpha) * currentPlayerLocY;
			}

		}
		circle(empty, Point(currentPlayerLocX, currentPlayerLocY), 4, Scalar(255, 0, 0), 6, 8, 0);
		putText(empty, "Size: " + std::to_string(allPossible[lowestIndex].containedPoints.size()), Point(10,30), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		putText(empty, "Score: " + std::to_string(lowestScore), Point(10,70), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,0));
		imshow("output", empty);
		key = waitKey(1);
	}
}
