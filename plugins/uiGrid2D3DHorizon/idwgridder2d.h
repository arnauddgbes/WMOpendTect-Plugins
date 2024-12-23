#pragma once

#include "wmgridder2d.h"
class uiParent;

class IDWGlobalInterpolator;
class IDWLocalInterpolator;

class wmIDWGridder2D : public wmGridder2D
{ mODTextTranslationClass(wmIDWGridder2D);
public:
    friend class IDWGlobalInterpolator;
    friend class IDWKNNInterpolator;
    friend class IDWLocalInterpolator;

    wmIDWGridder2D();

    bool	executeGridding(uiParent*);
protected:

};
