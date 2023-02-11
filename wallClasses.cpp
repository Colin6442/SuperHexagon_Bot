#include "wallClasses.h"
#include <opencv2/core.hpp>
#include <string.h>

using namespace cv;
constexpr auto π = 3.14159265;


angleDistance::angleDistance(){
	degree = 0;
	radian = 0;
}

angleDistance::~angleDistance(){}

angleDistance::angleDistance(int ang, double dist){
	degree = ang;
	radian = ang*(π/180);
	distances.push_back(dist);
}

void angleDistance::addWall(double dist){
	distances.push_back(dist);
	std::sort(distances.begin(), distances.end());
}

double angleDistance::getClosestWall(){
	return angleDistance::distances[0];
}

////////////////////////////////////////////////
// angleDistance ^




// Wall v
////////////////////////////////////////////////

Wall::Wall(Point pt1, Point pt2, double slopeIn, double toCenter, Point mid){
	point1 = pt1;
	point2 = pt2;
	length = sqrt(pow(pt1.x - pt2.x, 2) + pow(pt1.y - pt2.y, 2));
	slope = slopeIn;
	distToCenter = toCenter;
	middle = mid;
}

Wall::~Wall(){}

void Wall::getAnglesCovered(int width, int height, std::vector<angleDistance>& angles){
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

////////////////////////////////////////////////
// Wall ^




// wallSegment v
////////////////////////////////////////////////

wallSegment::wallSegment(){}

wallSegment::~wallSegment(){}

int compareAngle(double middleAng, double playerAng, std::string* dir) {
	double normal = abs(playerAng - middleAng);
	double add360 = playerAng - middleAng + 360;
	double minus360 = abs(playerAng - middleAng - 360);

	if (abs(int(playerAng) - middleAng) < 10) {
		*dir = "None";
		return 0;
	}

	if (normal <= add360 && normal <= minus360) {
		if (playerAng - middleAng > 0) {
			*dir = "Right";
		}
		else {
			*dir = "Left";
		}
		return abs(normal);
	}
	else if (add360 <= normal && add360 <= minus360) {
		*dir = "Right";
		return add360;
	}
	else if (minus360 <= normal && minus360 <= add360) {
		*dir = "Left";
		return minus360;
	}
	return 0;

}

wallSegment::wallSegment(int startIn, int endIn, int mid, double playerAngleDeg, double toCenter){
	start = startIn;
	end = endIn;
	middle = mid;
	distToCenter = toCenter;
	angleFromPlayer = compareAngle(middle, playerAngleDeg, &dirFromPlayer);
}