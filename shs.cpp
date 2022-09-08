#include <cstdio>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <iostream>

#include <vector>
#include <windows.h>
#include <Windows.h>

using namespace cv;

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

bool drawPlayerLoc(Mat& empty, Point pos, int width, int height) {
	double check = sqrt(pow(pos.x - width*.5, 2) + pow(pos.y - height*.5, 2));
	if (check > 60 && check < 75){
		//circle(empty, pos, 2, Scalar(255, 0, 0), 2, 8, 0);
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
	while (runbot){
		if (GetAsyncKeyState(VK_NUMPAD0)){
			runbot = false;
		}

		Mat original = getMat(hWND);
		Size s = original.size();
		int type = original.type();
		int width = s.width, height = s.height;
		//width: 768       height: 480
		Mat gray, edge;
		Mat empty(s, type);
		original.copyTo(gray);
		cvtColor(gray, gray, COLOR_BGR2GRAY);
		threshold(gray, gray, 200, 255, THRESH_BINARY);

		Canny(gray, edge, 10, 400, 3);

		//playerArea = width * .5 + -23, height * .5 + -23;
		//circle(empty, Point(width*.5, height*.5), 60, Scalar(255, 0, 0), 1, 8, 0);
		//circle(empty, Point(width*.5, height*.5), 75, Scalar(255, 0, 0), 1, 8, 0);
		//circle(empty, Point(width*.5, height*.5), 1, Scalar(255, 0, 0), 1, 8, 0);
		//line(empty, Point(0,height*.5), Point(width, height*.5), Scalar(255, 255, 255), 1, LINE_AA);
		//line(empty, Point(width*.5,0), Point(width*.5, height), Scalar(255, 255, 255), 1, LINE_AA);
		//line(empty, Point(522.5,0), Point(245.4, height), Scalar(255, 255, 255), 1, LINE_AA);
		//line(empty, Point(width,18.3), Point(0, 461.7), Scalar(255, 255, 255), 1, LINE_AA);
		//line(empty, Point(245.4,0), Point(522.5,height), Scalar(255, 255, 255), 1, LINE_AA);
		//line(empty, Point(0,18.3), Point(width, 461.7), Scalar(255, 255, 255), 1, LINE_AA);

		std::vector<Vec4i> lines;
		std::vector<Point> playerLoc;
		HoughLinesP(edge, lines, 1, CV_PI / 180, 7, 5, 10);
		int smallest = 99999;
		int length;
		for (size_t i = 0; i < lines.size(); i++){
			Vec4i l = lines[i];
			length = sqrt(pow((double)(l[0] - l[2]), 2) + pow((double)(l[1] - l[3]), 2));
			//circle(empty, Point(l[0], l[1]), 2, Scalar(0, 255, 0), 2, 8, 0);
			//circle(empty, Point(l[2], l[3]), 2, Scalar(0, 255, 0), 2, 8, 0);
			int x = abs(l[0] + l[2]) / 2, y = abs(l[1] + l[3]) / 2;
			Point pos = Point(x, y);
			//std::printf("in: %d, %d    out: %d, %d\n", x, y, pos.x, pos.y);
			if (drawPlayerLoc(empty, pos, width, height)) {
				playerLoc.push_back(pos);
			}
			line(empty, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0, 0, 255), 1, LINE_AA);	
		}
		int avgX = 0, avgY = 0;
		if (playerLoc.size() >= 1 && playerLoc.size() <= 3) {
			for (int i = 0; i < playerLoc.size(); i++) {
				avgX += playerLoc[i].x;
				avgY += playerLoc[i].y;
			}
			currentPlayerLocX = avgX / playerLoc.size();
			currentPlayerLocY = avgY / playerLoc.size();
		}
		circle(empty, Point(currentPlayerLocX, currentPlayerLocY), 4, Scalar(255, 0, 0), 4, 8, 0);
		imshow("output", empty);
		//imshow("edge", edge);
		key = waitKey(30);
	}
}
