#include "circularList.h"
#include <cstdlib>

circularList::circularList(int current, int size){
	this->size = size;
	this->current = current;
}

circularList& circularList::operator+=(const int& num){
	current = (current + num) % size;

	return *this;
}

circularList& circularList::operator-=(const int& num){
	current = (current + size - num) % size;
	
	return *this;
}

int circularList::difference(int num){
	int diff = abs(current - num);

	if(size - diff < diff){
		return (size - diff);
	}

	return diff;
}


