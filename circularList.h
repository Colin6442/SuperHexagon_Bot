#ifndef CIRCULAR_LIST_H
#define CIRCULAR_LIST_H

struct circularList{
	int size;
	int current;

	circularList(int current, int size);
	circularList& operator+=(const int& num);
	circularList& operator-=(const int& num);
};

// Example Usage
// bool circularListCheck(int at, int check, int thresh, int size){
// 	circularList begining(at, size);
// 	circularList ending(at, size);
// 	begining -= thresh;
// 	ending += thresh;
// 	while(begining.current != ending.current){
// 		if(begining.current == check){
// 			return true;
// 		}
// 		begining.current += 1;
// 	}
// 	return false;
//  }

#endif