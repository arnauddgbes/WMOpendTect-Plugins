#ifndef uihorizongrp_h
#define uihorizongrp_h

#include "uidlggroup.h"

class uiGenInput;
class uiCheckBox;
class uiLabeledComboBox;
class uiIOObjSel;
class uiSeis2DLineSelGrp;
class uiPosSubSel;
class MultiID;
class TrcKeyZSampling;

class uiHorizonGrp : public uiDlgGroup
{ mODTextTranslationClass(uiHorizonGrp);
public:
    uiHorizonGrp(uiParent*, bool has2Dhorizon=false, bool has3Dhorizon=false );
    ~uiHorizonGrp() {}
    
    bool doHorizonExport();
    const char* outputName();
    void getHorIds( MultiID& hor2Did, MultiID& hor3Did );
    void getGeoMids( TypeSet<Pos::GeomID>& geomids );
    void get3Dsel( TrcKeyZSampling& envelope );
    const char* attrib2D();
    const char* attrib3D();
    
    void update();

protected:
    uiGenInput*         namefld_;
    uiCheckBox*         exp2D_;
    uiIOObjSel*         hor2Dfld_;
    uiSeis2DLineSelGrp* lines2Dfld_;
    uiLabeledComboBox*  attrib2Dfld_;
    uiCheckBox*         exp3D_;
    uiIOObjSel*         hor3Dfld_;
    uiPosSubSel*        subsel3Dfld_;
    uiLabeledComboBox*  attrib3Dfld_;
    
    void                hor2Dsel(CallBacker*);
    void                hor3Dsel(CallBacker*);
    void                exp2Dsel(CallBacker*);
    void                exp3Dsel(CallBacker*);
    
};




#endif
