#include "wmgridder2d.h"

#include "uimsg.h"
#include "survinfo.h"
#include "posidxpair2coord.h"
#include "math2.h"
#include "emmanager.h"
#include "emobject.h"
#include "emhorizon2d.h"
#include "emhorizon3d.h"
#include "survgeom2d.h"
#include "posinfo2d.h"

typedef std::vector<std::size_t> TIds;

const char* wmGridder2D::sKeyRowStep()      { return "RowStep"; }
const char* wmGridder2D::sKeyColStep()      { return "ColStep"; }
const char* wmGridder2D::sKeyMethod()       { return "Method"; }
const char* wmGridder2D::sKeyPolyScopeNodeNr()   { return "PolyScopeNodeNr"; }
const char* wmGridder2D::sKeyPolyScopeNode()     { return "PolyScopeNode"; }
const char* wmGridder2D::sKeySearchRadius() { return "SearchRadius"; }
const char* wmGridder2D::sKeyScopeType()    { return "ScopeType"; }
const char* wmGridder2D::sKeyFaultID()      { return "FaultID"; }
const char* wmGridder2D::sKeyFaultNr()      { return "FaultNr"; }
const char* wmGridder2D::sKey2DHorizonID()  { return "2DHorizonID"; }
const char* wmGridder2D::sKey2DLineIDNr()   { return "2DLineIDNr"; }
const char* wmGridder2D::sKey2DLineID()     { return "2DLineID"; }
const char* wmGridder2D::sKey3DHorizonID()  { return "3DHorizonID"; }

const char* wmIDWGridder2D::sKeyPower()     { return "Power"; }

const char* wmGridder2D::MethodNames[] =
{
    "Inverse Distance Weighted Interpolation",
    0
};

const char* wmGridder2D::ScopeNames[] =
{
    "Bounding Box",
    "Convex Hull",
    "Polygon",
    0
};

wmGridder2D* wmGridder2D::create( const char* methodName )
{
    BufferString tmp(methodName);
    if (tmp == MethodNames[0])
        return (wmGridder2D*) new wmIDWGridder2D();
    else {
        ErrMsg("wmGridder2D::create - unrecognised method name");
        return nullptr;
    }
}

wmGridder2D::wmGridder2D() 
    : kdtree_(10)
    , searchradius_(mUdf(float))
    , grid_(0,0)
    , scopepoly_(nullptr)
{
    hs_ = SI().sampling( false ).hsamp_;
    hor2DID_.setUdf();
    hor3DID_.setUdf();
    inlrg_.setUdf();
    crlrg_.setUdf();
}

wmGridder2D::~wmGridder2D()
{
    if (scopepoly_!=nullptr)
        delete scopepoly_;
}

void wmGridder2D::setPoint( const Coord& loc, const float val )
{
    Coord pos = SI().binID2Coord().transformBackNoSnap(loc);
    locs_.push_back(TPoint(loc.x, loc.y));
    inlrg_.isUdf()? inlrg_.set(pos.x,pos.x) : inlrg_.include(pos.x);
    crlrg_.isUdf()? crlrg_.set(pos.y,pos.y) : crlrg_.include( pos.y );
    vals_.push_back(val);
}

bool wmGridder2D::usePar(const IOPar& par)
{
    hor2DID_.setUdf();
    geomids_.erase();
    if (par.get(sKey2DHorizonID(), hor2DID_)) {
        int nlines=0;
        par.get(sKey2DLineIDNr(), nlines);
        if (nlines>0) {
            for (int idx=0; idx<nlines; idx++) {
                Pos::GeomID id;
                if (par.get(IOPar::compKey(sKey2DLineID(),idx), id))
                    geomids_ += id;
            }
        }
    }
    
    hor3DID_.setUdf();
    if (par.get(sKey3DHorizonID(), hor3DID_))
        hor3Dsubsel_.usePar(par);
    
    int scopetype;
    par.get(sKeyScopeType(),scopetype);
    scope_ = (ScopeType)scopetype;
    
    if (scopepoly_!=nullptr) {
        delete scopepoly_;
        scopepoly_ = nullptr;
    }
    if (scope_==Polygon) {
        int nrnodes = 0;
        par.get(sKeyPolyScopeNodeNr(), nrnodes);
        if (nrnodes>0) {
            scopepoly_ = new ODPolygon<double>();
            for (int idx=0; idx<nrnodes; idx++) {
                Coord node;
                if (par.get(IOPar::compKey(sKeyPolyScopeNode(),idx), node))
                    scopepoly_->add(node);
            }
            scopepoly_->setClosed(true);
        }
    }
      
    par.get(sKeySearchRadius(), searchradius_);
    par.get(sKeyRowStep(), hs_.step_.first);
    par.get(sKeyColStep(), hs_.step_.second);
    
    faultids_.erase();
    int nrfaults = 0;
    if (par.get(sKeyFaultNr(), nrfaults)) {
        for (int idx=0; idx<nrfaults; idx++) {
            MultiID fltid;
            if (!par.get(IOPar::compKey(sKeyFaultID(),idx),fltid))
                return false;
            faultids_ += fltid;
        }
    }
    return true;
}

TrcKeySampling wmGridder2D::getTrcKeySampling() const
{
    return hs_;
}

void wmGridder2D::getHorRange(Interval<int>& inlrg, Interval<int>& crlrg)
{
    inlrg = Interval<int>(hs_.inlRange().start, hs_.inlRange().stop);
    crlrg = Interval<int>(hs_.crlRange().start, hs_.crlRange().stop);
}

bool wmGridder2D::saveGridTo(EM::Horizon3D* hor3d)
{
//    EM::SectionID sid = hor3d->geometry().sectionID( 0 );
//    hor3d->geometry().sectionGeometry(sid)->expandWithUdf(hs_.start_, hs_.stop_);
//    StepInterval<int> rowrg = hor3d->geometry().sectionGeometry(sid)->rowRange();
//    StepInterval<int> colrg = hor3d->geometry().sectionGeometry(sid)->colRange();
//    uiMSG().message(tr("saving grid\nInl: %1 - %2\nCrl: %3 - %4").arg(hs_.atIndex(0,0).first).arg(hs_.atIndex(grid_.info().getSize(0),0).first)
//    .arg(hs_.atIndex(0,0).second).arg(hs_.atIndex(0,grid_.info().getSize(1)).second));
   
    for( int idx=0; idx<grid_.info().getSize(0); idx++ ) {
        for (int idy=0; idy < grid_.info().getSize(1); idy++ ) {
            BinID bid = hs_.atIndex(idx,idy);
            TrcKey tk(bid);
            hor3d->setZ(tk, grid_.get(idx,idy), false);
        }
    }
    return true;
    uiMSG().message("grid saved");
}

bool wmGridder2D::loadData()
{
    if (!hor3DID_.isUdf()) {
        EM::EMObject* obj = EM::EMM().loadIfNotFullyLoaded(hor3DID_);
        if (!obj) {
            ErrMsg("wmGridder2D::loadData - loading 3D horizon failed");
            return false;
        }
        obj->ref();
        mDynamicCastGet(EM::Horizon3D*,hor,obj);
        if (!hor) {
           ErrMsg("wmGridder2D::loadData - casting 3D horizon failed");
           obj->unRef();
           return false;
        }
        for (int iln=hor3Dsubsel_.start_.inl(); iln<=hor3Dsubsel_.stop_.inl(); iln+=hor3Dsubsel_.step_.inl()) {
                for (int xln=hor3Dsubsel_.start_.crl(); xln<=hor3Dsubsel_.stop_.crl(); xln+=hor3Dsubsel_.step_.crl()) {
                    BinID bid(iln,xln);
                    TrcKey tk(bid);
                    const float z = hor->getZ( tk );
                    if (mIsUdf(z))
                        continue;
                    Coord coord;
                    coord = SI().transform(bid);
                    setPoint(coord, z);
                }
        }
        obj->unRef();
    }
    
    if (!hor2DID_.isUdf() && geomids_.size()>0) {
        EM::EMObject* obj = EM::EMM().loadIfNotFullyLoaded(hor2DID_);
        if (!obj) {
            ErrMsg("wmGridder2D::loadData - loading 2D horizon failed");
            return false;
        }
        obj->ref();
        mDynamicCastGet(EM::Horizon2D*,hor,obj);
        if (!hor) {
            ErrMsg("wmGridder2D - casting 2D horizon failed");
            obj->unRef();
            return false;
        }
        for (int idx=0; idx<geomids_.size(); idx++) {
            const StepInterval<int> trcrg = hor->geometry().colRange( geomids_[idx] );
            mDynamicCastGet(const Survey::Geometry2D*,survgeom2d,Survey::GM().getGeometry(geomids_[idx]))
            if (!survgeom2d || trcrg.isUdf() || !trcrg.step)
                continue;
                
            TrcKey tk( geomids_[idx], -1 );
            Coord crd;
            int spnr = mUdf(int);
            for ( int trcnr=trcrg.start; trcnr<=trcrg.stop; trcnr+=trcrg.step ) {
                tk.setTrcNr( trcnr );
                const float z = hor->getZ( tk );
                if (mIsUdf(z))
                    continue;
                    
                survgeom2d->getPosByTrcNr( trcnr, crd, spnr );
                setPoint(crd, z);
            }
        }
        obj->unRef();
    }
    return true;
}

bool wmGridder2D::setScope()
{
    if (scope_==BoundingBox) {
        Interval<int> inlrg((int)Math::Floor(inlrg_.start), (int)Math::Ceil(inlrg_.stop));
        Interval<int> crlrg((int)Math::Floor(crlrg_.start), (int)Math::Ceil(crlrg_.stop));
        hs_.setInlRange(inlrg);
        hs_.setCrlRange(crlrg);
    } else if (scope_==Polygon && scopepoly_!=nullptr) {
        ODPolygon<float> tmp;
        for (int idx=0; idx<scopepoly_->size(); idx++) {
            Coord pos = SI().binID2Coord().transformBackNoSnap(Coord(scopepoly_->getVertex(idx)));
            tmp.add(Geom::Point2D<float>(pos.x,pos.y));
        }
        Interval<float> inlrg = tmp.getRange(true);
        Interval<float> crlrg = tmp.getRange(false);
        hs_.setInlRange(Interval<int>((int) Math::Floor(inlrg.start), (int) Math::Ceil(inlrg.stop)));
        hs_.setCrlRange(Interval<int>((int) Math::Floor(crlrg.start), (int) Math::Ceil(crlrg.stop)));
    } else if (scope_==ConvexHull) {
// TODO - just use BoundingBox for now
        Interval<int> inlrg((int)Math::Floor(inlrg_.start), (int)Math::Ceil(inlrg_.stop));
        Interval<int> crlrg((int)Math::Floor(crlrg_.start), (int)Math::Ceil(crlrg_.stop));
        hs_.setInlRange(inlrg);
        hs_.setCrlRange(crlrg);
    } else
        return false;

//    uiMSG().message(tr("scope set\nInl: %1 - %2\nCrl: %3 - %4").arg(inlrg_.start).arg(inlrg_.stop)
//    .arg(hs_.crlRange().start).arg(hs_.crlRange().stop));

    return true;
}

bool wmGridder2D::grid()
{
    if (!loadData())
        return false;
    if (!setScope())
        return false;

    grid_.setSize( hs_.nrInl(), hs_.nrCrl() );
    grid_.setAll(mUdf(float));
    
    if (!mIsUdf(searchradius_)) {
        kdtree_.fill(locs_);
        uiMSG().message("kdtree built");
    }
    
    for( int idx=0; idx<grid_.info().getSize(0); idx++ ) {
        for (int idy=0; idy < grid_.info().getSize(1); idy++ ) {
            interpolate_( idx, idy );
        }
    }
    uiMSG().message("interpolation complete");

    return true;
}

wmIDWGridder2D::wmIDWGridder2D()
    : wmGridder2D()
    , pow_(2.0)
{}

bool wmIDWGridder2D::usePar(const IOPar& par)
{
    wmGridder2D::usePar(par);
    par.get(sKeyPower(), pow_);
    if (mIsUdf(pow_))
        pow_=2.0;
    return true;
}
  
void wmIDWGridder2D::interpolate_( const int ix, const int iy )
{
    const Pos::IdxPair2Coord& b2c = SI().binID2Coord();
    const BinID bid = hs_.atIndex(ix,iy);
    const Coord pos = b2c.transform(bid);
    double val = 0.0;
    double wgtsum = 0.0;
    if (!mIsUdf(searchradius_)) {
        TIds result;
        kdtree_.range( pos.x-searchradius_, pos.y-searchradius_, pos.x+searchradius_, pos.y+searchradius_, [&result](const std::size_t id) { result.push_back(id); });
        if (result.size() == 0)
            return;
        double r2 = searchradius_*searchradius_;
        for (int idx=0; idx<result.size(); idx++) {
            TPoint locs = locs_[result[idx]];
            double vals = vals_[result[idx]];
            double d = (locs.first-pos.x)*(locs.first-pos.x)+(locs.second-pos.y)*(locs.second-pos.y);
            if ( mIsZero(d,mDefEpsF)) {
                grid_.set(ix, iy, vals);
                return;
            }
            if (d<=r2) {
                double wgt = Math::PowerOf(d, -0.5*pow_);
                val += vals * wgt;
                wgtsum += wgt;
            }
        }
    } else {
        for (int idx=0; idx<locs_.size(); idx++) {
            double d = (locs_[idx].first-pos.x)*(locs_[idx].first-pos.x)+(locs_[idx].second-pos.y)*(locs_[idx].second-pos.y);
            if ( mIsZero(d,mDefEpsF)) {
                grid_.set(ix, iy, vals_[idx]);
                return;
            }
            double wgt = Math::PowerOf(d, -0.5*pow_);
            val += vals_[idx] * wgt;
            wgtsum += wgt;
        }
    }
    val /= wgtsum;
    grid_.set(ix, iy, (float) val);
}
