#include "wmgridder2d.h"
#include "mbagridder2d.h"
#include "rbfgridder2d.h"
#include "idwgridder2d.h"

#include "uimsg.h"
#include "bufstring.h"
#include "uistring.h"
#include "survinfo.h"
#include "posidxpair2coord.h"
#include "math2.h"
#include "emmanager.h"
#include "emobject.h"
#include "emhorizon2d.h"
#include "emhorizon3d.h"
#include "emioobjinfo.h"
#include "survgeom2d.h"
#include "posinfo2d.h"
#include "ioman.h"
#include "picklocation.h"
#include "pickset.h"
#include "picksettr.h"


#ifndef M_LOG_2
#define M_LOG_2 0.69314718055994530942
#endif
#ifndef M_GAMMA
#define M_GAMMA 0.577215664901532860606512
#endif

typedef std::vector<std::size_t> TIds;

const char* wmGridder2D::sKeyScopeHorID()   { return "ScopeHorID"; }
const char* wmGridder2D::sKeyRowStep()      { return "RowStep"; }
const char* wmGridder2D::sKeyColStep()      { return "ColStep"; }
const char* wmGridder2D::sKeyMethod()       { return "Method"; }
const char* wmGridder2D::sKeyClipPolyID()   { return "ClipPolyID"; }
const char* wmGridder2D::sKeyBlockSize()    { return "BlockSize"; }
const char* wmGridder2D::sKeyOverlap()      { return "PercentOverlap"; }
const char* wmGridder2D::sKeyScopeType()    { return "ScopeType"; }
const char* wmGridder2D::sKeyFaultPolyID()  { return "FaultPolyID"; }
const char* wmGridder2D::sKeyFaultPolyNr()  { return "FaultPolyNr"; }
const char* wmGridder2D::sKeyFaultID()      { return "FaultID"; }
const char* wmGridder2D::sKeyFaultNr()      { return "FaultNr"; }
const char* wmGridder2D::sKey2DHorizonID()  { return "2DHorizonID"; }
const char* wmGridder2D::sKey2DLineIDNr()   { return "2DLineIDNr"; }
const char* wmGridder2D::sKey2DLineID()     { return "2DLineID"; }
const char* wmGridder2D::sKey3DHorizonID()  { return "3DHorizonID"; }
const char* wmGridder2D::sKeyRegularization()    { return "Regularization"; }
const char* wmGridder2D::sKeyTension()      { return "Tension"; }

const char* wmGridder2D::MethodNames[] =
{
    "Local Radial Basis Functions",
    "Multilevel B-Splines",
    "Inverse Distance Weighted",
    0
};

const char* wmGridder2D::ScopeNames[] =
{
    "Bounding Box",
    "Horizon",
    0
};

wmGridder2D* wmGridder2D::create( const char* methodName )
{
    BufferString tmp(methodName);
    if (tmp == MethodNames[LocalRBF])
        return (wmGridder2D*) new wmRBFGridder2D();
    else if (tmp == MethodNames[IDW])
	return (wmGridder2D*) new wmIDWGridder2D();
    else if (tmp == MethodNames[MBA])
	return (wmGridder2D*) new wmMBAGridder2D();
    else {
        ErrMsg("wmGridder2D::create - unrecognised method name");
        return nullptr;
    }
}

bool wmGridder2D::canHandleFaultPolygons( const char* methodName )
{
    BufferString tmp(methodName);
    if (tmp == MethodNames[LocalRBF])
	return true;
    else if (tmp == MethodNames[IDW])
	return true;
    else if (tmp == MethodNames[MBA])
	return false;
    else {
	ErrMsg("wmGridder2D::canHandleFaultPolygons - unrecognised method name");
	return false;
    }
}

wmGridder2D::wmGridder2D() 
    : kdtree_(10)
    , blocksize_(mUdf(float))
    , grid_(0)
    , carr_(0)
    , tr_(nullptr)
{
    hs_ = SI().sampling( false ).hsamp_;
    hor2DID_.setUdf();
    hor3DID_.setUdf();
    croppolyID_.setUdf();
    horScopeID_.setUdf();
    inlrg_.setUdf();
    crlrg_.setUdf();
}

wmGridder2D::~wmGridder2D()
{
    if (carr_) {
        delete carr_;
        carr_ = 0;
    }
}

// loc is in survey grid coordinates
void wmGridder2D::setPoint( const Coord& binLoc, const float val )
{
    binLocs_.push_back(TPoint(binLoc.x, binLoc.y));
    vals_.push_back(val);
    includeInRange(binLoc);
}

void wmGridder2D::includeInRange(const Coord& pos)
{
    if (inlrg_.isUdf()) {
	inlrg_.set(pos.x,pos.x);
	crlrg_.set(pos.y,pos.y);
    } else {
	inlrg_.include(pos.x);
	crlrg_.include( pos.y );
    }
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
    
    horScopeID_.setUdf();
    par.get(sKeyScopeHorID(), horScopeID_);
    
    croppolyID_.setUdf();
    par.get(sKeyClipPolyID(), croppolyID_);
    
    int nrfaultpoly = 0;
    faultpolyID_.erase();
    if (par.get(sKeyFaultPolyNr(), nrfaultpoly)) {
        for (int idx=0; idx<nrfaultpoly; idx++) {
            MultiID id;
            if (!par.get(IOPar::compKey(sKeyFaultPolyID(), idx), id))
                return false;
            faultpolyID_ += id;
        }
    }
    
    par.get(sKeyBlockSize(), blocksize_);
    par.get(sKeyOverlap(), percoverlap_);
    
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
	hor3d->geometry().sectionGeometry(0)->setArray(hs_.start_, hs_.step_, grid_, true);
    return true;
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
                Coord coord(iln, xln);
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
            int spnr = mUdf(int);
            for ( int trcnr=trcrg.start; trcnr<=trcrg.stop; trcnr+=trcrg.step ) {
                tk.setTrcNr( trcnr );
                const float z = hor->getZ( tk );
                if (mIsUdf(z))
                    continue;

                Coord coord;
                survgeom2d->getPosByTrcNr( trcnr, coord, spnr );
		Coord binLoc = SI().binID2Coord().transformBackNoSnap(coord);
                setPoint(binLoc, z);
            }
        }
        obj->unRef();
    }
    
    if (!croppolyID_.isUdf()) {
        croppoly_.erase();
        PtrMan<IOObj> ioobj = IOM().get(croppolyID_);
        if (!ioobj) {
            ErrMsg("wmGridder2D::loadData - cannot get crop polygon ioobj");
            return false;
        }
        Pick::Set ps;
        BufferString msg;
        if (!PickSetTranslator::retrieve(ps, ioobj, true, msg)) {
            BufferString tmp("wmGridder2D::loadData - error reading crop polygon - ");
            tmp += msg;
            ErrMsg(tmp);
            return false;
        }
        for (int idx=0; idx<ps.size(); idx++) {
            const Pick::Location& pl = ps[idx];
	    Coord binLoc = SI().binID2Coord().transformBackNoSnap(pl.pos_.coord());
            croppoly_.add(binLoc);
        }
        croppoly_.setClosed(true);
    }
    
    faultpoly_.erase();
    for (int idx=0; idx<faultpolyID_.size(); idx++) {
        ODPolygon<Pos::Ordinate_Type>* fault = new ODPolygon<Pos::Ordinate_Type>();
        PtrMan<IOObj> ioobj = IOM().get(faultpolyID_[idx]);
        if (!ioobj) {
            ErrMsg("wmGridder2D::loadData - cannot get fault polygon ioobj");
            return false;
        }
        Pick::Set ps;
        BufferString msg;
        if (!PickSetTranslator::retrieve(ps, ioobj, true, msg)) {
            BufferString tmp("wmGridder2D::loadData - error reading fault polygon - ");
            tmp += msg;
            ErrMsg(tmp);
            return false;
        }
        for (int idp=0; idp<ps.size(); idp++) {
            const Pick::Location& pl = ps[idp];
	    Coord binLoc = SI().binID2Coord().transformBackNoSnap(pl.pos_.coord());
	    fault->add(binLoc);
        }
        faultpoly_ += fault;
    }
    return true;
}

bool wmGridder2D::setScope()
{
    if (scope_==BoundingBox) {
        StepInterval<int> inlrg((int)Math::Floor(inlrg_.start), (int)Math::Ceil(inlrg_.stop), hs_.step_.first);
		inlrg.stop = inlrg.atIndex(inlrg.indexOnOrAfter(Math::Ceil(inlrg_.stop), mDefEps));
        StepInterval<int> crlrg((int)Math::Floor(crlrg_.start), (int)Math::Ceil(crlrg_.stop), hs_.step_.second);
		crlrg.stop = crlrg.atIndex(crlrg.indexOnOrAfter(Math::Ceil(crlrg_.stop), mDefEps));
        hs_.setInlRange(Interval<int>(inlrg.start,inlrg.stop));
        hs_.setCrlRange(Interval<int>(crlrg.start, crlrg.stop));
    } else if (scope_==Horizon && !horScopeID_.isUdf()) {
        EM::IOObjInfo eminfo(horScopeID_);
        hs_.setInlRange(eminfo.getInlRange());
        hs_.setCrlRange(eminfo.getCrlRange());
    } else
        return false;

    return true;
}

bool wmGridder2D::isUncropped( Coord pos ) const
{
    if (!croppolyID_.isUdf())
        return croppoly_.isInside( pos, true, mDefEpsD);
    else
        return true;
}

bool wmGridder2D::inFaultHeave(Coord pos) const
{
    for (int idx=0; idx<faultpoly_.size(); idx++) {
        if (faultpoly_[idx]->isInside(pos, false, mDefEpsD))
            return true;
    }
    return false;
}

bool wmGridder2D::segmentsIntersect( Coord s1p1, Coord s1p2, Coord s2p1, Coord s2p2 )
{
    Interval<Pos::Ordinate_Type> ix1(s1p2.x, s1p1.x);
    Interval<Pos::Ordinate_Type> ix2(s2p2.x, s2p1.x);
    Interval<Pos::Ordinate_Type> iy1(s1p2.y, s1p1.y);
    Interval<Pos::Ordinate_Type> iy2(s2p2.y, s2p1.y);
    
    if (ix1.overlaps(ix2) && iy1.overlaps(iy2)) {
        double denom = ((s2p2.y-s2p1.y)*(s1p2.x-s1p1.x))-((s2p2.x-s2p1.x)*(s1p2.y-s1p1.y));
        if (mIsZero(denom, mDefEps))
            return false;
        double ua = ((s2p2.x-s2p1.x)*(s1p1.y-s2p1.y)-(s2p2.y-s2p1.y)*(s1p1.x-s2p1.x))/denom;
        double ub = ((s1p2.x-s1p1.x)*(s1p1.y-s2p1.y)-(s1p2.y-s1p1.y)*(s1p1.x-s2p1.x))/denom;
        return ((ua >= 0.0) && (ua <= 1.0) && (ub >= 0.0) && (ub <= 1.0));
    }
    return false;
}

bool wmGridder2D::faultBetween(Coord s1p1, Coord s1p2) const
{
    for (int idx=0; idx<faultpoly_.size(); idx++) {
        for (int iv=0; iv<faultpoly_[idx]->size(); iv++) {
            Coord s2p1 = faultpoly_[idx]->getVertex(iv);
            Coord s2p2 = faultpoly_[idx]->nextVertex(iv);
            if (segmentsIntersect(s1p1, s1p2, s2p1, s2p2))
               return true;
        }
    }
    return false;
}

bool wmGridder2D::prepareForGridding()
{
    if (!loadData())
	return false;
    if (!setScope())
	return false;
    if (!grid_) {
	grid_ = new Array2DImpl<float>(hs_.nrInl(), hs_.nrCrl());
	if (!grid_) {
	    ErrMsg("wmGridder2D::prepareForGridding - allocation of array for grid failed");
	    return false;
	}
    } else
	grid_->setSize(hs_.nrInl(), hs_.nrCrl());
    
    grid_->setAll(mUdf(float));
    
    return true;
}

mDefParallelCalc2Pars( LocalInterpolator, od_static_tr("LocalInterpolator","Interpolate nearest grid points"),
		       const wmGridder2D*, interp, Threads::Lock, lock )
mDefParallelCalcBody( 
const std::vector<TPoint>& locs_ = interp_->binLocs_;
const std::vector<float>& vals_ = interp_->vals_;
const TrcKeySampling& hs_ = interp_->hs_;
Array2DImpl<float>* grid_ = interp_->grid_;
Array2DImpl<float>* carr_ = interp_->carr_;
,
const Coord pos(locs_[idx].first, locs_[idx].second);
BinID bidSnap = hs_.getNearest(BinID(mNINT32(pos.x), mNINT32(pos.y)));
if (mIsEqual(pos.x, bidSnap.inl(), mDefEps) && mIsEqual(pos.y, bidSnap.crl(), mDefEps)) {
    int ix = hs_.inlIdx(bidSnap.inl());
    int iy = hs_.crlIdx(bidSnap.crl());
    if (ix>=0 && ix<hs_.nrInl() && iy>=0 && iy<hs_.nrCrl()) {
	Threads::Locker lckr( lock_ );
	double prev = mIsUdf(grid_->get(ix,iy)) ? 0.0 : grid_->get(ix,iy);
	grid_->set(ix, iy, prev + vals_[idx]);
	carr_->set(ix, iy, carr_->get(ix, iy) + 1.0);
    }
} else {
    BinID r[4];
    r[0] = pos.x<bidSnap.inl() ? bidSnap-BinID(hs_.step_.inl(),0) : bidSnap;
    r[1] = r[0] + BinID(hs_.step_.inl(),0);
    r[2] = pos.y<bidSnap.crl() ? r[0]-BinID(0,hs_.step_.crl()) : r[0];
    r[3] = r[2] + BinID(hs_.step_.inl(),0);
    for (int ir=0; ir<4; ir++) {
	Coord rpos(r[ir].inl(), r[ir].crl());
	if (!interp_->faultBetween(pos, rpos)) {
	    int ix = hs_.inlIdx(r[ir].inl());
	    int iy = hs_.crlIdx(r[ir].crl());
	    if (ix>=0 && ix<hs_.nrInl() && iy>=0 && iy<hs_.nrCrl()) {
		double dist = rpos.sqHorDistTo(pos);
		double wgt = tanh(dist)/dist;
		Threads::Locker lckr( lock_ );
		double prev = mIsUdf(grid_->get(ix,iy)) ? 0.0 : grid_->get(ix,iy);
		grid_->set(ix, iy, prev + wgt*vals_[idx]);
		carr_->set(ix, iy, carr_->get(ix, iy) + wgt);
	    }
	}
    }
}
, )

void wmGridder2D::localInterp()
{
    carr_ = new Array2DImpl<float>(hs_.nrInl(), hs_.nrCrl());
    if (!carr_) {
	ErrMsg("wmGridder2D::prepareForGridding - allocation of array for confidence grid failed");
	return;
    }
    carr_->setAll(0.0);
    
    Threads::Lock lock;
    LocalInterpolator interp( binLocs_.size(), this, lock);
    interp.execute();
    
    binLocs_.clear();
    vals_.clear();
    interpidx_.erase();
    
    for (od_int64 idx=0; idx<hs_.totalNr(); idx++) {
	BinID gridBid = hs_.atIndex(idx);
	int ix = hs_.inlIdx(gridBid.inl());
	int iy = hs_.crlIdx(gridBid.crl());
	float carr = carr_->get(ix,iy);
	double x = gridBid.inl();
	double y = gridBid.crl();
	Coord gridPos(x, y);
	if (isUncropped(gridPos) && !inFaultHeave(gridPos)) {
	    if (carr!=0.0) {
		float val = grid_->get(ix, iy)/carr;
		grid_->set(ix, iy, val);
		vals_.push_back( val );
		binLocs_.push_back(TPoint(x, y));
	    } else
		interpidx_ += idx;
	} else
	    grid_->set(ix, iy, mUdf(float));
    }
}

/*
wmLCCTSGridder2D::wmLCCTSGridder2D()
: wmGridder2D()
, tension_(0.25)
, minvals_(7)
, maxvals_(20)
{}

bool wmLCCTSGridder2D::usePar(const IOPar& par)
{
    par.get(sKeyTension(), tension_);
    par.get(sKeyMaxVal(), maxvals_);
    if (maxvals_<minvals_)
	maxvals_ = minvals_;
    return wmGridder2D::usePar(par);
}

bool wmLCCTSGridder2D::prepareForGridding(TaskRunner* tr)
{
    bool res = wmGridder2D::prepareForGridding( tr );
// Set up some local constants
    double gs = (hs_.lineDistance() + hs_.trcDistance())/2.0;
    p0_ = sqrt(tension_/(1.0-tension_))/gs;
    p1_ = 2.0/p0_;
    return res;
}

double wmLCCTSGridder2D::basis_(const double r) const
{
    double g = 0.0;
    double y, z, t, cx;
    if (!mIsZero(r, mDefEps)) {
        cx = p0_*r;
        if (r <= p1_) {
            y = 0.25 * (t = cx*cx);
            z = t/14.0625;
            g = (-log(0.5*cx) * (z * (3.5156229 + z * (3.0899424 + z * (1.2067492 + z * (0.2659732 +
                z * (0.360768e-1 + z * 0.45813e-2))))))) + (y * (0.42278420 + y * (0.23069756 +
                y * (0.3488590e-1 + y * (0.262698e-2 + y * (0.10750e-3 + y * 0.74e-5))))));
        } else {
            y = p1_/r;
            g = (exp (-cx) / sqrt (cx)) * (1.25331414 + y * (-0.7832358e-1 + y * (0.2189568e-1 +
                y * (-0.1062446e-1 + y * (0.587872e-2 + y * (-0.251540e-2 + y * 0.53208e-3))))))
                + log (cx) - M_LOG_2 + M_GAMMA;
        }
    }
    
    return g;
}


void wmLCCTSGridder2D::interpolate_( int ix, int iy, Coord pos )
{
    if (!mIsUdf(searchradius_) && carr_->get(ix,iy)==0.0) {
        TIds result;
        kdtree_.range( pos.x-searchradius_, pos.y-searchradius_, 
                       pos.x+searchradius_, pos.y+searchradius_, [&result](const std::size_t id) { result.push_back(id); });
        if (result.size() == 0)
            return;
        // Prune points across faults
        for (auto it=result.begin(); it!=result.end();) {
            Coord loc(locs_[*it].first, locs_[*it].second);
            if (faultBetween(pos, loc)) {
                result.erase(it);
                continue;
            } else {
                it++;
            }
        }
        int num = result.size();
        if (num < minvals_) {
            return;
        }
        num = num<maxvals_ ? num : maxvals_;
        
        Eigen::MatrixXd M = Eigen::MatrixXd::Zero(num+3, num+3);
        Eigen::ArrayXd d(num);
        for (int idx=0; idx<num; idx++) {
            Coord loc(locs_[result[idx]].first, locs_[result[idx]].second);
            d[idx] = pos.horDistTo(loc);
            for (int idy=idx+1; idy<num; idy++) {
                Coord q(locs_[result[idy]].first, locs_[result[idy]].second);
                double dist = loc.horDistTo(q);
                M(idx,idy) = basis_(dist);
                M(idy,idx) = M(idx,idy);
            }
        }
        
        for (int idx=0; idx<num; idx++) {
            M(idx,idx) = 0.0;
            M(idx, num) = 1.0;
            M(idx, num+1) = locs_[result[idx]].first;
            M(idx, num+2) = locs_[result[idx]].second;
            M(num, idx) = 1.0;
            M(num+1, idx) = locs_[result[idx]].first;
            M(num+2, idx) = locs_[result[idx]].second;
        }
        
        for (int idx=num; idx<num+3; idx++)
            for (int idy=num; idy<num+3;idy++)
                M(idx, idy) = 0.0;
            
            Eigen::VectorXd V = Eigen::VectorXd::Zero(num+3);
        for (int idx=0; idx<num; idx++)
            V(idx) = vals_[result[idx]];
        
        Eigen::VectorXd W =  M.colPivHouseholderQr().solve(V);
        
        for (int idx=0; idx<num; idx++)
            d[idx] = basis_(d[idx]);
        double val = W(num) + W(num+1)*pos.x + W(num+2)*pos.y;
        val += (W.head(num).array() * d).sum();
        
        grid_->set(ix, iy, (float)val);
    }
}
*/
