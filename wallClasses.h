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
	void getAnglesCovered(int width, int height, std::vector<angleDistance>& angles);
};

struct wallSegment{
	int start;
	int middle;
	int end;
	double distToCenter;
	double distFromPlayer;
	cv::String dirFromPlayer;

	wallSegment();
	wallSegment(int startIn, int endIn, double playerAngleRad, double toCenter);
};



#endif