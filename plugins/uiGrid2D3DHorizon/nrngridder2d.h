#pragma once

#include "wmgridder2d.h"

// Nearest Neighbour gridder

class wmNRNGridder2D : public wmGridder2D
{
public:
    wmNRNGridder2D();
    ~wmNRNGridder2D() {}
    
    bool		executeGridding(TaskRunner*);
    
};


