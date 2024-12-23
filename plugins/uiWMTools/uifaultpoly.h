#pragma once
/*
 *   Fault polygon/polyline generator class
 *   Copyright (C) 2019  Wayne Mogg
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "uidialog.h"
#include "commontypes.h"
#include "polygon.h"
#include "coord.h"

namespace Pick {
    class Set;
};

namespace Geometry {
    class BinIDSurface;
};

class Callbacker;
class uiColorInput;
class uiGenInput;
class uiIOObjSel;
class uiFaultParSel;
namespace EM {
    class Horizon3D;
};


mClass(uiWMTools) uiFaultPoly : public uiDialog
{ mODTextTranslationClass(uiFaultPoly);
public:
    uiFaultPoly(uiParent*);
    ~uiFaultPoly();

    bool	addToDisplay() const;
    int		nrFaults() const;
    RefMan<Pick::Set>	getPolyForFault( int idx );
    // Becomes yours
    Coord3	lineSegmentIntersection(Coord3, Coord3,
					const Geometry::BinIDSurface*) const;

protected:

    uiGenInput*			prefixfld_;
    uiColorInput*		colorfld_;
    uiIOObjSel*			hor3Dfld_;
    uiFaultParSel*		faultsfld_;

    BufferString		prefix_;
    EM::Horizon3D*		hor_;

    bool                acceptOK(CallBacker*);

private:
    uiString    getCaptionStr() const;
};
