#ifndef WALL_CLASSES_H
#define WALL_CLASSES_H
#include <vector>
#include <opencv2/core.hpp>
#include <string.h>

struct angleDistance{
	int degree;
	double radian;
	std::vector<double> distances;

	angleDistance();
	~angleDistance();
	angleDistance(int ang, double dist);
	void addWall(double dist);
	double getClosestWall();

};

struct Wall{
	double distToCenter;
	double slope;
	cv::Point point1;
	cv::Point point2;
	cv::Point middle;
	double length;

	Wall(cv::Point pt1, cv::Point pt2, double slopeIn, double toCenter, cv::Point mid);
	~Wall();
	void getAnglesCovered(int width, int height, std::vector<angleDistance>& angles);
};

struct wallSegment{
	int start;
	int middle;
	int end;
	double distToCenter;
	double angleFromPlayer;
	std::string dirFromPlayer;
	int score = 0;

	wallSegment();
	~wallSegment();
	wallSegment(int startIn, int endIn, int mid, double playerAngleDeg, double toCenter);
};



#endif