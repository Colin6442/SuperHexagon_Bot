#include "circularList.h"

circularList::circularList(int current, int size){
	this->size = size;
	this->current = current;
}

circularList& circularList::operator+=(const int& num){
	int fixed = num % size;
	if (current + fixed <= size - 1 && current + fixed >= 0) {
		current += fixed;
	}
	else if (current + fixed > size - 1) {
		current = current + fixed - size;
	}
	else if (current + fixed < 0) {
		current = size + current + fixed;
	}

	return *this;
}

circularList& circularList::operator-=(const int& num){
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


