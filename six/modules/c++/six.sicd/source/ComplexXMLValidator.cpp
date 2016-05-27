/* =========================================================================
* This file is part of six.sicd-c++
* =========================================================================
*
* (C) Copyright 2004 - 2014, MDA Information Systems LLC
*
* six.sicd-c++ is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more messageBuilder.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this program; If not,
* see <http://www.gnu.org/licenses/>.
*
*/

#include <six/sicd/ComplexXMLValidator.h>
#include <six/sicd/Functor.h>


namespace six
{
namespace sicd
{

ComplexXMLValidator::ComplexXMLValidator(const ComplexData& data, logging::Logger* log) :
    sicd(data),
    mLog(log)
{
    if (mLog == NULL)
    {
        throw except::Exception(Ctxt("Null pointer to logger passed to ComplexXMLValidator ctor"));
    }
}

void ComplexXMLValidator::fillDerivedFields(ComplexData& data, bool setDefaultValues)
{
    if (data.grid.get() != NULL && data.grid->row.get() != NULL)
    {
        fillRowCol(*data.grid->row);
    }
    if (data.grid.get() != NULL && data.grid->col.get() != NULL)
    {
        fillRowCol(*data.grid->col);
    }

    fillSCPTime(data);
    fillARPPoly(data);
    fillRadarCollection(data, setDefaultValues);
    fillGeoDataSCP(data);
    fillSCPCOA(data);

    double fc = Init::undefined<double>();
    if (data.imageFormation.get() != NULL &&
        data.radarCollection.get() != NULL &&
        data.radarCollection->refFrequencyIndex == 0)
    {
        fc = (data.imageFormation->txFrequencyProcMin + data.imageFormation->txFrequencyProcMax) / 2;
    }

    fillImageFormationAlgorithm(data, fc, setDefaultValues);
    fillGeoData(data);
    return;
}

void ComplexXMLValidator::fillGeoData(ComplexData& data)
{
    if (data.geoData.get() == NULL)
    {
        data.geoData.reset(new GeoData());
    }

    if (Init::isUndefined<LatLon>(data.geoData->imageCorners.getCorner(0)) &&
        data.imageData.get() != NULL &&
        !Init::isUndefined<size_t>(data.imageData->numRows) &&
        !Init::isUndefined<size_t>(data.imageData->numCols))
    {
        std::vector<RowColDouble> cornerLineSample;
        cornerLineSample.resize(4);
        cornerLineSample[0].row = 0;
        cornerLineSample[0].col = 0;
        cornerLineSample[1].row = 0;
        cornerLineSample[1].col = data.imageData->numCols - 1;
        cornerLineSample[2].row = data.imageData->numRows - 1;
        cornerLineSample[2].col = data.imageData->numCols - 1;
        cornerLineSample[3].row = data.imageData->numRows - 1;
        cornerLineSample[4].col = 0;
        // line 746; requires point_slant_to_ground
    }

    // Derived: Add ValidData geocoords
    if (data.imageData.get() != NULL &&
        !data.imageData->validData.empty() &&
        data.geoData->validData.empty())
    {
        // line 757; requires point slant to ground
    }
}

void ComplexXMLValidator::fillImageFormationAlgorithm(ComplexData& data, double fc, bool setDefaultValues)
{
    if (!(data.scpcoa.get() != NULL &&
        !Init::isUndefined<Vector3>(data.scpcoa->arpPos) &&
        !Init::isUndefined<Vector3>(data.scpcoa->arpVel) &&
        data.geoData.get() != NULL &&
        !Init::isUndefined<Vector3>(data.geoData->scp.ecf) &&
        data.imageFormation.get() != NULL))
    {
        return;
    }
    switch (data.imageFormation->imageFormationAlgorithm)
    {
    case ImageFormationType::RGAZCOMP:
        fillRGAZCOMP(data, fc);
        break;
    case ImageFormationType::PFA:
        fillPFA(data, fc, setDefaultValues);
        break;
    case ImageFormationType::RMA:
        fillRMA(data, fc, setDefaultValues);
    }
}

void ComplexXMLValidator::fillRMA(ComplexData& data, double fc, bool setDefaultValues)
{
    if (data.rma.get() == NULL)
        return;

    Vector3& scp = data.geoData->scp.ecf;

    if (data.rma->rmat.get() != NULL ||
        data.rma->rmcr.get() != NULL)
    {
        if (setDefaultValues)
        {
            if (data.grid->imagePlane == ComplexImagePlaneType::NOT_SET)
            {
                data.grid->imagePlane = ComplexImagePlaneType::SLANT;
            }
            if (data.grid->type == ComplexImageGridType::NOT_SET)
            {
                if (data.rma->rmat.get() != NULL)
                {
                    data.grid->type = ComplexImageGridType::XCTYAT;
                }
                else if (data.rma->rmcr.get() != NULL)
                {
                    data.grid->type = ComplexImageGridType::XRGYCR;
                }
            }

            // Default: set PosRef/VelRef to  SCPCOA Pos/Vel
            if (data.rma->rmat.get() != NULL)
            {
                if (Init::isUndefined<Vector3>(data.rma->rmat->refPos))
                {
                    // fillSCPTime() will always end with an initialized SCPCOA
                    data.rma->rmat->refPos = data.scpcoa->arpPos;
                }
                if (Init::isUndefined<Vector3>(data.rma->rmat->refVel))
                {
                    data.rma->rmat->refVel = data.scpcoa->arpVel;
                }
            }
            else if (data.rma->rmcr.get() != NULL)
            {
                if (Init::isUndefined<Vector3>(data.rma->rmcr->refPos))
                {
                    data.rma->rmcr->refPos = data.scpcoa->arpPos;
                }
                if (Init::isUndefined<Vector3>(data.rma->rmcr->refVel))
                {
                    data.rma->rmcr->refVel = data.scpcoa->arpVel;
                }
            }

            // Default: RMAT/RMCR Row/Col.kCenter
            if (!Init::isUndefined<double>(fc))
            {
                if (data.grid->row.get() == NULL)
                {
                    data.grid->row.reset(new DirectionParameters());
                }
                if (data.grid->col.get() == NULL)
                {
                    data.grid->col.reset(new DirectionParameters());
                }
                double kfc = fc * 2 / math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC;
                if (data.rma->rmat.get() != NULL)
                {
                    
                    if (Init::isUndefined<double>(data.grid->row->kCenter))
                    {
                        data.grid->row->kCenter = kfc * std::sin(data.rma->rmat->dopConeAngleRef * math::Constants::RADIANS_TO_DEGREES);
                    }
                    
                    if (Init::isUndefined<double>(data.grid->col->kCenter))
                    {
                        data.grid->col->kCenter = kfc * std::cos(data.rma->rmat->dopConeAngleRef * math::Constants::RADIANS_TO_DEGREES);
                    }
                }
                else if (data.rma->rmcr.get() != NULL)
                {
                    if (Init::isUndefined<double>(data.grid->row->kCenter))
                    {
                        data.grid->row->kCenter = kfc;
                    }
                    if (Init::isUndefined<double>(data.grid->col->kCenter))
                    {
                        data.grid->col->kCenter = 0;
                    }
                }
            }
        }

        if (data.rma->rmat.get() != NULL && 
            !Init::isUndefined<Vector3>(data.rma->rmat->refPos) &&
            !Init::isUndefined<Vector3>(data.rma->rmat->refVel))
        {
            Vector3 refPos = data.rma->rmat->refPos;
            Vector3 refVel = data.rma->rmat->refVel;
            Vector3 uLOS = (scp - refPos).unit();
            Vector3 left = cross(refPos.unit(), refVel.unit());
            int look = sign(left.dot(uLOS));

            // RCA is a derived field
            if (Init::isUndefined<double>(data.rma->rmat->dopConeAngleRef))
            {
                data.rma->rmat->dopConeAngleRef = std::acos(refVel.unit().dot(uLOS)) * math::Constants::RADIANS_TO_DEGREES;
            }

            // Row/Col.UnitVector and Derived fields
            if (Init::isUndefined<Vector3>(data.grid->row->unitVector) &&
                Init::isUndefined<Vector3>(data.grid->col->unitVector))
            {
                Vector3 uYAT = refVel.unit() * -look;
                Vector3 spn = cross(uLOS, uYAT).unit();
                Vector3 uXCT = cross(uYAT, spn);
                data.grid->row->unitVector = uXCT;
                data.grid->col->unitVector = uYAT;
            }
        }
        
        else if (data.rma->rmcr.get() != NULL &&
            !Init::isUndefined<Vector3>(data.rma->rmcr->refPos) &&
            !Init::isUndefined<Vector3>(data.rma->rmcr->refVel))
        {
            Vector3 refPos = data.rma->rmcr->refPos;
            Vector3 refVel = data.rma->rmcr->refVel;
            Vector3 uLOS = (scp - refPos).unit();
            Vector3 left = cross(refPos.unit(), refVel.unit());
            int look = sign(left.dot(uLOS));

            // RCA is a derived field
            if (Init::isUndefined<double>(data.rma->rmcr->dopConeAngleRef))
            {
                data.rma->rmcr->dopConeAngleRef = std::acos(refVel.unit().dot(uLOS)) * math::Constants::RADIANS_TO_DEGREES;
            }

            // Row/Col.UnitVector and Derived fields
            if (Init::isUndefined<Vector3>(data.grid->row->unitVector) &&
                Init::isUndefined<Vector3>(data.grid->col->unitVector))
            {
                Vector3 uXRG = uLOS;
                Vector3 spn = cross(refVel.unit(), uXRG).unit() * look ;
                Vector3 uYCR = cross(spn, uXRG);
                data.grid->row->unitVector = uXRG;
                data.grid->col->unitVector = uYCR;
            }
        }
    }
    else if (data.rma->inca.get() != NULL)
    {
        // Default: RGZERO grid is the natural result of RMA/INCA
        if (data.grid->type == ComplexImageGridType::NOT_SET)
        {
            data.grid->type = ComplexImageGridType::RGZERO;
        }
        if (!Init::isUndefined<Poly1D>(data.rma->inca->timeCAPoly) &&
            data.position.get() != NULL &&
            !Init::isUndefined<PolyXYZ>(data.position->arpPoly))
        {
            // INCA UVects are DERIVED from closest approach position/
            // velocity, not center of aperture
            Vector3 caPos = data.position->arpPoly(data.rma->inca->timeCAPoly(1));
            Vector3 caVel = data.position->arpPoly.derivative()(data.rma->inca->timeCAPoly(1));

            if (Init::isUndefined<double>(data.rma->inca->rangeCA))
            {
                data.rma->inca->rangeCA = (caPos - scp).norm();
            }

            if (Init::isUndefined<Vector3>(data.grid->row->unitVector) &&
                Init::isUndefined<Vector3>(data.grid->col->unitVector))
            {
                Vector3 uRG = (scp - caPos).unit();
                Vector3 left = cross(caPos.unit(), caVel.unit());
                int look = sign(left.dot(uRG));
                Vector3 spn = cross(uRG, caVel).unit() * -look;
                Vector3 uAC = cross(spn, uRG);
                data.grid->row->unitVector = uRG;
                data.grid->col->unitVector = uAC;
            }
        }

        // Derived: Always the case for INCA
        if (Init::isUndefined<double>(data.grid->col->kCenter))
        {
            data.grid->col->kCenter = 0;
        }

        // Default: The frequency used for computing Doppler Centroid values
        // is often the center transmitted frequency
        if (setDefaultValues &&
            data.radarCollection.get() != NULL &&
            !Init::isUndefined<double>(data.radarCollection->txFrequencyMin) &&
            !Init::isUndefined<double>(data.radarCollection->txFrequencyMax) &&
            Init::isUndefined<double>(data.rma->inca->freqZero))
        {
            data.rma->inca->freqZero = (data.radarCollection->txFrequencyMin + 
                    data.radarCollection->txFrequencyMax ) / 2;
        }

        // Row.kCenter/FreqZero Derived relationship is exact
        // although freqZero may be set to default above
        if (!Init::isUndefined<double>(data.rma->inca->freqZero) &&
            Init::isUndefined<double>(data.grid->row->kCenter))
        {
            data.grid->row->kCenter = data.rma->inca->freqZero * 2 /
                    math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC;
        }
    }
    return;
}

void ComplexXMLValidator::fillPFA(ComplexData& data, double fc, bool setDefaultValues)
{
    Vector3& scp = data.geoData->scp.ecf;
    Vector3& arpPos = data.scpcoa->arpPos;
    Vector3& arpVel = data.scpcoa->arpVel;

    Vector3 uLOS = (scp - arpPos).unit();
    Vector3 left = cross(arpPos, arpVel).unit();
    int look = sign(uLOS.dot(left));
    Vector3 slantPlaneNormal = cross(arpVel, uLOS).unit() * look;
    Vector3 etp = wgs84Norm(scp);

    // DEFAULT: RGAZIM grid is the natural result of PFA
    if (setDefaultValues)
    {
        if (data.grid.get() == NULL)
        {
            data.grid.reset(new Grid());
        }
        if (data.grid->type == ComplexImageGridType::NOT_SET)
        {
            data.grid->type = ComplexImageGridType::RGAZIM;
        }

        if (data.pfa.get() == NULL)
        {
            data.pfa.reset(new PFA());
        }
        if (Init::isUndefined<Vector3>(data.pfa->imagePlaneNormal))
        {
            switch (data.grid->imagePlane)
            {
            case ComplexImagePlaneType::SLANT:
            case ComplexImagePlaneType::NOT_SET:
                data.pfa->imagePlaneNormal = slantPlaneNormal;
                break;
            case ComplexImagePlaneType::GROUND:
                data.pfa->imagePlaneNormal = etp;
                break;
            case ComplexImagePlaneType::OTHER:
                // Nothing we can do
                break;
            }
        }
        if (Init::isUndefined<Vector3>(data.pfa->focusPlaneNormal))
        {
            data.pfa->focusPlaneNormal = etp;
        }
    }
    Vector3 polRefPos;
    if (data.position.get() != NULL &&
        !Init::isUndefined<PolyXYZ>(data.position->arpPoly) &&
        data.pfa.get() != NULL &&
        !Init::isUndefined<double>(data.pfa->polarAngleRefTime))
    {
        std::vector<double> coefs;
        // Compute exactly if possible
        for (size_t ii = 0; ii < 3; ++ii)
        {
            Poly1D poly = polyAt(data.position->arpPoly, ii);
            coefs.push_back(poly(data.pfa->polarAngleRefTime));
        }
        polRefPos = Vector3(coefs);
    }
    else if (setDefaultValues)
    {
        // Otherwise, guess PolarAngleRefTime = SCPTime
        polRefPos = arpPos;
        if (data.scpcoa.get() != NULL && !Init::isUndefined<double>(data.scpcoa->scpTime))
        {
            data.pfa->polarAngleRefTime = data.scpcoa->scpTime;
        }
    }

    // Lines 503-522 require polyfit, pfa_polar_coords

    // Lines 524 - 550 require bsxfun

    // DEFAULT value. Almost always zero for PFA
    if (setDefaultValues)
    {
        if (data.grid->col.get() == NULL)
        {
            data.grid->col.reset(new DirectionParameters());
        }
        if (Init::isUndefined<double>(data.grid->col->kCenter))
        {
            data.grid->col->kCenter = 0;
        }

        if (data.grid->row.get() == NULL)
        {
            data.grid->row.reset(new DirectionParameters());
        }
        if (Init::isUndefined<double>(data.grid->row->kCenter))
        {
            if (!Init::isUndefined<double>(data.pfa->krg1) &&
                !Init::isUndefined<double>(data.pfa->krg2))
            {
                // Default: the most reasonable way to compute this
                data.grid->row->kCenter = (data.pfa->krg1 + data.pfa->krg2) / 2;
            }
            else if (!Init::isUndefined<double>(fc))
            {
                // Approximation: this may not be quite right, due to
                // rectangular inscription loss in PFA, but it should
                // be close.
                data.grid->row->kCenter = fc * (2 / math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC) *
                    data.pfa->spatialFrequencyScaleFactorPoly(1);
            }
        }
    }
    return;
}

double ComplexXMLValidator::nonZero(double arg)
{
    return arg == 0 ? arg : std::numeric_limits<double>::epsilon();
}

void ComplexXMLValidator::fillRGAZCOMP(ComplexData& data, double fc)
{
    Vector3& scp = data.geoData->scp.ecf;
    Vector3& arpPos = data.scpcoa->arpPos;
    Vector3& arpVel = data.scpcoa->arpVel;

    Vector3 uLOS = (scp - arpPos).unit();
    Vector3 left = cross(arpPos, arpVel).unit();
    int look = sign(uLOS.dot(left));
    Vector3 slantPlaneNormal = cross(arpVel, uLOS).unit() * look;

    // Derived: RGAZCOMP image formation must result in a SLANT, RGAZIM grid
    if (data.grid.get() == NULL)
    {
        data.grid.reset(new Grid());
    }
    if (data.grid->imagePlane == ComplexImagePlaneType::NOT_SET)
    {
        data.grid->imagePlane = ComplexImagePlaneType::SLANT;
    }
    if (data.grid->imagePlane == ComplexImageGridType::NOT_SET)
    {
        data.grid->type = ComplexImageGridType::RGAZIM;
    }

    // Derived: RgAzComp.AzSF
    if (data.rgAzComp.get() == NULL)
    {
        data.rgAzComp.reset(new RgAzComp());
    }
    if (Init::isUndefined<double>(data.rgAzComp->azSF))
    {
        
        data.rgAzComp->azSF = std::sin(data.scpcoa->dopplerConeAngle * math::Constants::DEGREES_TO_RADIANS) /
            data.scpcoa->slantRange;
    }

    // Dervied: RgAzComp.KazPoly
    if (data.timeline.get() != NULL &&
        data.timeline->interPulsePeriod.get() != NULL &&
        data.timeline->interPulsePeriod->sets.size() == 1 &&
        !Init::isUndefined<Poly1D>(data.timeline->interPulsePeriod->sets[0].interPulsePeriodPoly) &&
        data.grid->row.get() != NULL &&
        !Init::isUndefined<double>(data.grid->row->kCenter) &&
        Init::isUndefined<Poly1D>(data.rgAzComp->kazPoly))
    {
        double krgCoa = data.grid->row->kCenter;
        if (!Init::isUndefined<Poly2D>(data.grid->row->deltaKCOAPoly))
        {
            krgCoa += data.grid->row->deltaKCOAPoly.atY(scp[1])(scp[0]);
        }
        double stRateCoa = data.timeline->interPulsePeriod->sets[0].
            interPulsePeriodPoly.derivative()(data.scpcoa->scpTime);
        double deltaKazPerDeltaV = look * krgCoa * arpVel.norm() * std::sin(data.scpcoa->dopplerConeAngle * math::Constants::DEGREES_TO_RADIANS)
            / nonZero(data.scpcoa->slantRange) / nonZero(stRateCoa);
        data.rgAzComp->kazPoly = data.timeline->interPulsePeriod->sets[0].interPulsePeriodPoly * deltaKazPerDeltaV;
    }

    // Derived UVectECF
    if (data.grid->row.get() == NULL)
    {
        data.grid->row.reset(new DirectionParameters());
    }
    if (data.grid->col.get() == NULL)
    {
        data.grid->col.reset(new DirectionParameters());
    }
    if (Init::isUndefined<Vector3>(data.grid->row->unitVector))
    {
        data.grid->row->unitVector = uLOS;
    }
    if (Init::isUndefined<Vector3>(data.grid->col->unitVector))
    {
        data.grid->col->unitVector = cross(slantPlaneNormal, uLOS);
    }
    // Derived: Kctr/DeltaKCOAPoly
    // In SICD, if the optional DeltaKCOAPoly field is omitted,
    // it is assumed to be zero. If the creator of the partial 
    // SICD metadata just forgot it, or didn't know it, rather
    // than leaving the field off as an explicit declaration of
    // a zero value, the KCtr computation will be wrong if the
    // DFT was not "centered" (s_0 = s_coa and v_0 = v_coa in
    // the terminology of the SICD spec).
    if (!Init::isUndefined<double>(fc))
    {
        // We've already guaranteed grid, row, col exists, so not
        // checking for NULL
        if (Init::isUndefined<double>(data.grid->row->kCenter))
        {
            if (!Init::isUndefined<Poly2D>(data.grid->row->deltaKCOAPoly))
            {
                data.grid->row->kCenter = fc * 2 / math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC - 
                    data.grid->row->deltaKCOAPoly.atY(scp[1])(scp[0]);
            }
            else
            {
                data.grid->row->kCenter = fc * 2 / math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC;
            }
        }
        else if (!Init::isUndefined<Poly2D>(data.grid->row->deltaKCOAPoly))
        {
            // KCtr populated, but not DeltaKCOAPoly
            std::vector<double> coefs(1, fc * 2 / math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC - data.grid->row->kCenter);
            data.grid->row->deltaKCOAPoly = Poly2D(0, 0, coefs);
        }
    }
    if (Init::isUndefined<double>(data.grid->col->kCenter))
    {
        if (!Init::isUndefined<Poly2D>(data.grid->col->deltaKCOAPoly))
        {
            // DeltaKCOAPoly populated, but not KCtr (would be odd)
            data.grid->col->kCenter = -data.grid->row->deltaKCOAPoly.atY(scp[1])(scp[0]);
        }
    }
    else if (Init::isUndefined<Poly2D>(data.grid->col->deltaKCOAPoly))
    {
        // KCtr popualted, but not DeltaKCOAPoly
        std::vector<double> coefs(1, -data.grid->col->kCenter);
        data.grid->col->deltaKCOAPoly = Poly2D(0, 0, coefs);
    }
    return;
}

void ComplexXMLValidator::fillSCPCOA(ComplexData& data)
{
    // We're deriving fields from ARPPos, ARPVel, and SCP
    // So make sure to call fillARPPoly() and fillGeoDataSCP
    // before this function

    if (!(data.scpcoa.get() != NULL &&
        !Init::isUndefined<Vector3>(data.scpcoa->arpPos) &&
        !Init::isUndefined<Vector3>(data.scpcoa->arpVel) &&
        data.geoData.get() != NULL &&
        !Init::isUndefined<Vector3>(data.geoData->scp.ecf)))
    {
        return;
    }

    Vector3& scp = data.geoData->scp.ecf;
    Vector3& arpPos = data.scpcoa->arpPos;
    Vector3& arpVel = data.scpcoa->arpVel;

    Vector3 uLOS = (scp - arpPos) / (scp - arpPos).norm();
    Vector3 left = cross(arpPos / arpPos.norm(), arpVel / arpPos.norm());
    int look = sign(uLOS.dot(left));

    if (data.scpcoa->sideOfTrack == SideOfTrackType::NOT_SET)
    {
        if (look < 0)
        {
            data.scpcoa->sideOfTrack = SideOfTrackType::RIGHT;
        }
        else
        {
            data.scpcoa->sideOfTrack = SideOfTrackType::LEFT;
        }
    }

    if (Init::isUndefined<double>(data.scpcoa->slantRange))
    {
        data.scpcoa->slantRange = (scp - arpPos).norm();
    }
    if (Init::isUndefined<double>(data.scpcoa->groundRange))
    {
        data.scpcoa->groundRange = (scp.norm() * std::acos((arpPos.dot(scp)) / (scp.norm() * arpPos.norm())));
    }
    if (Init::isUndefined<double>(data.scpcoa->dopplerConeAngle))
    {
        data.scpcoa->dopplerConeAngle = std::acos((arpVel / arpVel.norm()).dot(uLOS)) * math::Constants::RADIANS_TO_DEGREES;
    }

    // Earth Tangent Plane (ETP) at the SCP is the plane tangent to the
    // surface of constant height above the WGS 84 ellipsoid (HAE) that
    // contains the SCP. The ETP is an approximation to the ground plane
    // at the SCP.
    Vector3 etp = wgs84Norm(scp);
    if (Init::isUndefined<double>(data.scpcoa->grazeAngle))
    {
        // Angle between ground plane and line-of-sight vector
        data.scpcoa->grazeAngle = std::asin(etp.dot(uLOS * -1)) * math::Constants::RADIANS_TO_DEGREES;
    }
    if (Init::isUndefined<double>(data.scpcoa->incidenceAngle))
    {
        // Angle between ground plane normal and line of sight vector
        data.scpcoa->incidenceAngle = 90 - data.scpcoa->grazeAngle;
    }

    // Instantaneous slant plane unit normal at COA
    // (also called uSPZ in SICD spec)
    Vector3 slantPlaneNormal = cross(arpVel, uLOS).unit() * look;

    // Project range vector (from SCP toward ARP) onto ground plane
    Vector3 uGPX = (uLOS * -1) - etp * (etp.dot(uLOS * -1));
    uGPX = uGPX.unit();

    if (Init::isUndefined<double>(data.scpcoa->twistAngle))
    {
        // 1) Equations from SICD spec:
        Vector3 uGPY = cross(etp, uGPX);

        // Angle from +GPY axis to the +SPY axis in plane of incidence
        data.scpcoa->twistAngle = -1 * std::asin(uGPY.dot(slantPlaneNormal)) * math::Constants::RADIANS_TO_DEGREES;
    }

    if (Init::isUndefined<double>(data.scpcoa->slopeAngle))
    {
        // Angle between slant and ground planes
        data.scpcoa->slopeAngle = std::acos(etp.dot(slantPlaneNormal)) * math::Constants::RADIANS_TO_DEGREES;
    }

    std::vector<double> coordinates;
    coordinates.resize(3);
    coordinates[0] = 0;
    coordinates[1] = 0;
    coordinates[2] = 1;
    Vector3 zeroZeroOne(coordinates);

    // Project north onto ground plane
    Vector3 northGround = zeroZeroOne - (etp * etp.dot(zeroZeroOne));
    // Unit vector in ground plane in north direction
    Vector3 uNorth = northGround.unit();
    // Unit vector in ground plane in east direction
    Vector3 uEast = cross(uNorth, etp);

    if (Init::isUndefined<double>(data.scpcoa->azimAngle))
    {
        double azNorth = uGPX.dot(uNorth);
        double azEast = uGPX.dot(uEast);
        data.scpcoa->azimAngle = std::fmod(std::atan2(azEast, azNorth) * 
                math::Constants::RADIANS_TO_DEGREES, 360);
    }

    if (Init::isUndefined<double>(data.scpcoa->layoverAngle))
    {
        Vector3 layoverGround = etp - (slantPlaneNormal / (etp.dot(slantPlaneNormal)));
        double loNorth = layoverGround.dot(uNorth);
        double loEast = layoverGround.dot(uEast);
        data.scpcoa->layoverAngle = std::fmod(std::atan2(loEast, loNorth) *
            math::Constants::RADIANS_TO_DEGREES, 360);
    }
}

void ComplexXMLValidator::fillGeoDataSCP(ComplexData& data)
{
    if (data.geoData.get() != NULL &&
        !Init::isUndefined<Vector3>(data.geoData->scp.ecf) &&
        Init::isUndefined<LatLonAlt>(data.geoData->scp.llh))
    {
        scene::ECEFToLLATransform transformer;
        data.geoData->scp.llh = transformer.transform(data.geoData->scp.ecf);
    }
    if (data.geoData.get() != NULL &&
        !Init::isUndefined<LatLonAlt>(data.geoData->scp.llh) &&
        Init::isUndefined<Vector3>(data.geoData->scp.ecf))
    {
        scene::LLAToECEFTransform transformer;
        data.geoData->scp.ecf = transformer.transform(data.geoData->scp.llh);
    }
}

Vector3 ComplexXMLValidator::wgs84Norm(const Vector3& point)
{
    scene::WGS84EllipsoidModel model;
    std::vector<double> coordinates;
    coordinates.resize(3);

    coordinates[0] = point[0] / std::pow(model.getEquatorialRadius(), 2);
    coordinates[1] = point[1] / std::pow(model.getEquatorialRadius(), 2);
    coordinates[2] = point[2] / std::pow(model.getPolarRadius(), 2);

    Vector3 normal(coordinates);
    return normal / normal.norm();
}

void ComplexXMLValidator::fillRadarCollection(ComplexData& data, bool setDefaultValues)
{
    if (data.radarCollection.get() == NULL)
    {
        return;
    }

    // Transmit bandwidth
    if (!data.radarCollection->waveform.empty())
    {
        // DERIVED: These values should be equal
        if (Init::isUndefined<double>(data.radarCollection->txFrequencyMin))
        {
            double derivedMin = std::numeric_limits<double>::infinity();
            for (size_t ii = 0; ii < data.radarCollection->waveform.size(); ++ii)
            {
                if (data.radarCollection->waveform[ii].get() != NULL)
                {
                    derivedMin = std::min(derivedMin, data.radarCollection->waveform[ii]->txFrequencyStart);
                }
            }
            data.radarCollection->txFrequencyMin = derivedMin;
        }
        if (Init::isUndefined<double>(data.radarCollection->txFrequencyMax))
        {
            double derivedMax = -std::numeric_limits<double>::infinity();
            for (size_t ii = 0; ii < data.radarCollection->waveform.size(); ++ii)
            {
                if (data.radarCollection->waveform[ii].get() != NULL)
                {
                    derivedMax = std::max(derivedMax, 
                        data.radarCollection->waveform[ii]->txFrequencyStart + 
                        data.radarCollection->waveform[ii]->txRFBandwidth);
                }
            }
            data.radarCollection->txFrequencyMax = derivedMax;
        }
        for (size_t ii = 0; ii < data.radarCollection->waveform.size(); ++ii)
        {
            if (data.radarCollection->waveform[ii].get() == NULL)
            {
                continue;
            }

            WaveformParameters wfParameters = *data.radarCollection->waveform[ii];
            if (wfParameters.rcvDemodType == DemodType::CHIRP &&
                Init::isUndefined<double>(wfParameters.rcvFMRate))
            {
                wfParameters.rcvFMRate = 0;
            }
            
            if (wfParameters.rcvFMRate == 0 &&
                wfParameters.rcvDemodType == DemodType::NOT_SET)
            {
                wfParameters.rcvDemodType = DemodType::CHIRP;
            }

            if (Init::isUndefined<double>(wfParameters.txRFBandwidth) &&
                !Init::isUndefined<double>(wfParameters.txPulseLength) &&
                !Init::isUndefined<double>(wfParameters.txFMRate))
            {
                wfParameters.txRFBandwidth = wfParameters.txPulseLength * wfParameters.txFMRate;
            }

            if (!Init::isUndefined<double>(wfParameters.txRFBandwidth) &&
                Init::isUndefined<double>(wfParameters.txPulseLength) &&
                !Init::isUndefined<double>(wfParameters.txFMRate))
            {
                if (wfParameters.txFMRate != 0)
                {
                    wfParameters.txPulseLength = wfParameters.txRFBandwidth / wfParameters.txFMRate;
                }
            }

            if (!Init::isUndefined<double>(wfParameters.txRFBandwidth) &&
                !Init::isUndefined<double>(wfParameters.txPulseLength) &&
                Init::isUndefined<double>(wfParameters.txFMRate))
            {
                if (wfParameters.txPulseLength != 0)
                {
                    wfParameters.txFMRate = wfParameters.txRFBandwidth / wfParameters.txPulseLength;
                }
            }
        }
    }

    if (!Init::isUndefined<double>(data.radarCollection->txFrequencyMin) &&
        !Init::isUndefined<double>(data.radarCollection->txFrequencyMax))
    {
        // Default: we often assume that all transmitted bandwidth was
        // processed, if given no other information
        //TODO: move to fillImageFormation()
        if (setDefaultValues)
        {
            if (data.imageFormation.get() == NULL)
            {
                data.imageFormation.reset(new ImageFormation());
            }
            if (Init::isUndefined<double>(data.imageFormation->txFrequencyProcMin))
            {
                data.imageFormation->txFrequencyProcMin = data.radarCollection->txFrequencyMin;
            }
            if (Init::isUndefined<double>(data.imageFormation->txFrequencyProcMax))
            {
                data.imageFormation->txFrequencyProcMax = data.radarCollection->txFrequencyMax;
            }
        }

        //Derived: These values should be equal.
        if (data.radarCollection->waveform.size() == 1 &&
            data.radarCollection->waveform[0].get() != NULL)
        {
            if (Init::isUndefined<double>(data.radarCollection->waveform[0]->txFrequencyStart))
            {
                data.radarCollection->waveform[0]->txFrequencyStart = data.radarCollection->txFrequencyMin;
            }
            if (Init::isUndefined<double>(data.radarCollection->waveform[0]->txRFBandwidth))
            {
                data.radarCollection->waveform[0]->txRFBandwidth = 
                    data.radarCollection->txFrequencyMax - data.radarCollection->txFrequencyMin;
            }
        }
    }

    //ImageFormation @ 272
    return;
}

void ComplexXMLValidator::fillARPPoly(ComplexData& data)
{
    // ARP Pos/Vel/Acc fields can be derived from ARPPoly and SCPTime
    if (data.position.get() != NULL &&
        !Init::isUndefined<PolyXYZ>(data.position->arpPoly) &&
        data.scpcoa.get() != NULL &&
        !Init::isUndefined<double>(data.scpcoa->scpTime))
    {
        std::vector<double> arpPosEcf;
        std::vector<double> arpVelEcf;
        std::vector<double> arpAccEcf;
        for (size_t ii = 0; ii < 3; ++ii)
        {
            Poly1D posPoly = polyAt(data.position->arpPoly, ii);
            Poly1D velPoly = posPoly.derivative();
            Poly1D accPoly = velPoly.derivative();

            arpPosEcf.push_back(posPoly(data.scpcoa->scpTime));
            arpVelEcf.push_back(velPoly(data.scpcoa->scpTime));
            arpAccEcf.push_back(accPoly(data.scpcoa->scpTime));
        }
        if (Init::isUndefined<Vector3>(data.scpcoa->arpPos))
        {
            data.scpcoa->arpPos = Vector3(arpPosEcf);
        }
        
        if (Init::isUndefined<Vector3>(data.scpcoa->arpVel))
        {
            data.scpcoa->arpVel = Vector3(arpVelEcf);
        }

        if (Init::isUndefined<Vector3>(data.scpcoa->arpAcc))
        {
            data.scpcoa->arpAcc = Vector3(arpAccEcf);
        }
    }

    // A simple ARPPoly can be derived from SCPCOA Pos/Vel/Acc if that was 
    // all that was defined
    if (data.scpcoa.get() != NULL &&
        !Init::isUndefined<Vector3>(data.scpcoa->arpPos) &&
        !Init::isUndefined<Vector3>(data.scpcoa->arpVel) &&
        !Init::isUndefined<double>(data.scpcoa->scpTime) &&
        (data.position.get() == NULL || Init::isUndefined<PolyXYZ>(data.position->arpPoly)))
    {
        if (Init::isUndefined<Vector3>(data.scpcoa->arpAcc))
        {
            data.scpcoa->arpAcc = Vector3(0.0);
        }

        std::vector<Vector3> coefs;
        coefs.resize(3);

        //constant
        coefs[0] = data.scpcoa->arpPos - 
            data.scpcoa->arpVel * data.scpcoa->scpTime +
            (data.scpcoa->arpAcc / 2) * std::pow(data.scpcoa->scpTime, 2);

        //linear
        coefs[1] = data.scpcoa->arpVel - 
            data.scpcoa->arpAcc * data.scpcoa->scpTime;

        //quadratic
        coefs[2] = data.scpcoa->arpAcc / 2;

        data.position->arpPoly = PolyXYZ(coefs);
    }
    return;
}

void ComplexXMLValidator::fillSCPTime(ComplexData& data)
{
    // SCPTime can always be derived from Grid.TimeCOAPoly
    if (data.scpcoa.get() == NULL)
    {
        data.scpcoa.reset(new SCPCOA());
    }
    if (Init::isUndefined<double>(data.scpcoa->scpTime) &&
        data.grid.get() != NULL &&
        !Init::isUndefined<Poly2D>(data.grid->timeCOAPoly))
    {
        data.scpcoa->scpTime = data.grid->timeCOAPoly.atY(1)(1);
    }

    // and sometimes Grid.TimeCOAPoly can be derived from SCPTime
    if (!Init::isUndefined<double>(data.scpcoa->scpTime) &&
        data.collectionInformation.get() != NULL &&
        data.collectionInformation->radarMode.toString() == "SPOTLIGHT")
    {
        if (data.grid.get() == NULL)
        {
            data.grid.reset(new Grid());
        }
        if (Init::isUndefined<Poly2D>(data.grid->timeCOAPoly))
        {
            // I'm assuming this is what it means to assign a double to a Poly2D...
            data.grid->timeCOAPoly = Poly2D(1, 1);
            data.grid->timeCOAPoly[0][0] = data.scpcoa->scpTime;
        }
    }
}

void ComplexXMLValidator::fillRowCol(DirectionParameters& rowCol)
{
    if (rowCol.weightType.get() != NULL && 
        rowCol.weights.empty() &&
        (rowCol.weightType->windowName != "UNIFORM" &&
            rowCol.weightType->windowName != "UNKNOWN"))
    {
        size_t defaultWgtSize = 512;
        rowCol.weights = (*calculateWeightFunction(rowCol))(defaultWgtSize);
    }

    // Calulating resolution requires fzero and fft functions

    // DeltaK1/2 are approximated from DeltaKCOAPoly
    if (!Init::isUndefined(rowCol.deltaKCOAPoly) &&
        !Init::isUndefined(rowCol.impulseResponseBandwidth) &&
        !Init::isUndefined(rowCol.sampleSpacing) &&
        (Init::isUndefined(rowCol.deltaK1) && Init::isUndefined(rowCol.deltaK2)) &&
        sicd.imageData.get() != NULL)
    {
        // Here, we assume the min and max of DeltaKCOAPoly must be on the vertices
        // of the image, since it is smooth and monotonic in most cases--although in
        // actuality this is not always the case. To be totally generic, we would 
        // have to search for an interior min and max as well.

        std::vector<std::vector<sys::SSize_T> > vertices = calculateImageVertices();
        std::vector<double> deltas = calculateDeltaKs(rowCol, vertices);

        rowCol.deltaK1 = deltas[0];
        rowCol.deltaK2 = deltas[1];
    }
}

bool ComplexXMLValidator::validate()
{
    return (
        checkFFTSigns() &&                       // 2.2
        checkFrequencySupportParameters() &&     // 2.3.1  - 2.3.9
        checkSupportParamsAgainstPFA() &&        // 2.3.10 - 2.3.16
        checkWeightFunctions() &&                // 2.4  (omitting 2.5: requires fft, fzero)
        checkARPPoly() &&                        // 2.6  (omitting 2.7: requries derived_sicd_fields.m)
        checkWaveformDescription() &&            // 2.8  (omitting 2.9: required add_sicd_corners.m)
        checkGeoData() &&                        // 2.10
        checkValidData() &&                      // 2.11
        checkIFP());                             // 2.12
}

bool ComplexXMLValidator::checkIFP()
{
    ImageFormationType formType = sicd.imageFormation->imageFormationAlgorithm;

    switch (formType)
    {
    case ImageFormationType::RGAZCOMP: // 2.12.1
        return checkRGAZCOMP();
        break;
    case ImageFormationType::PFA:      // 2.12.2
        return checkPFA();
        break;
    case ImageFormationType::RMA:      // 2.12.3.*
        return checkRMA();
        break;
    default:
        //2.12.3 (This is not a typo)
        messageBuilder.str("");
        messageBuilder << "Image formation not fully defined." << std::endl
            << "SICD.ImageFormation.ImageFormAlgo = OTHER or is not set.";
        mLog->warn(messageBuilder.str());
        return false;
        break;
    }
}

bool ComplexXMLValidator::checkRGAZCOMP()
{
    bool valid = true;

    // 2.12.1.1
    if (sicd.grid->imagePlane != ComplexImagePlaneType::SLANT)
    {
        messageBuilder.str("");
        messageBuilder << "RGAZCOMP image formation should result in a SLANT plane image." << std::endl
            << "Grid.ImagePlane: " << sicd.grid->imagePlane.toString();
        mLog->error(messageBuilder.str());
        valid = false;
    }

    //2.12.1.2
    if (sicd.grid->type != ComplexImageGridType::RGAZIM)
    {
        messageBuilder.str("");
        messageBuilder << "RGAZCOMP image formation should result in a RGAZIM grid." << std::endl
            << "Grid.Type: " << sicd.grid->type.toString();
        mLog->error(messageBuilder.str());
        valid = false;
    }

    //2.12.1.8
    if (!Init::isUndefined(sicd.grid->col->deltaKCOAPoly) &&
        sicd.grid->col->deltaKCOAPoly.orderX() == 1 && sicd.grid->col->deltaKCOAPoly.orderY() == 1
        && std::abs(sicd.grid->col->kCenter - (-sicd.grid->col->deltaKCOAPoly[0][0])) > std::numeric_limits<double>::epsilon())
    {
        messageBuilder.str("");
        messageBuilder << "Grid.Col.KCenter must be equal to -Grid.Col.DeltaKCOAPoly for RGAZCOMP data." << std::endl
            << "Grid.Col.KCenter: " << sicd.grid->col->kCenter << std::endl
            << "Grid.Col.DeltaKCOAPoly: " << sicd.grid->col->deltaKCOAPoly[0][0];
        mLog->error(messageBuilder.str());
        valid = false;
    }
    else if (sicd.grid->col->kCenter != 0)
    {
        messageBuilder.str("");
        messageBuilder << "Grid.Col.KCetner must be zero for RGAZCOMP data with no "
            << "Grid.Col.DeltaKCOAPoly fied" << std::endl
            << "Grid.Col.KCenter: " << sicd.grid->col->kCenter;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    //2.12.1.9
    if (sicd.radarCollection.get() == NULL || Init::isUndefined(sicd.radarCollection->refFrequencyIndex))
    {
        double fcProc = (sicd.imageFormation->txFrequencyProcMin + sicd.imageFormation->txFrequencyProcMax) / 2;
        double kfc = fcProc * (2 / math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC);
        if (!Init::isUndefined(sicd.grid->row->deltaKCOAPoly))
        {
            Poly2D poly = sicd.grid->row->deltaKCOAPoly;
            for (size_t ii = 0; ii < poly.orderX(); ++ii)
            {
                for (size_t jj = 0; jj < poly.orderY(); ++jj)
                {
                    if (std::abs(sicd.grid->row->kCenter - (kfc - poly[ii][jj]) > std::numeric_limits<double>::epsilon()))
                    {
                        messageBuilder.str("");
                        messageBuilder << WF_INCONSISTENT_STR
                            << "Grid.Col.KCenter: " << sicd.grid->col->kCenter << std::endl
                            << "Center frequency * 2/c - Grid.Row.DeltaKCOAPoly: "
                            << kfc - poly[ii][jj];
                        mLog->error(messageBuilder.str());
                        valid = false;
                        break;
                    }
                }
            }
        }
        else if (std::abs(sicd.grid->row->kCenter - kfc) > std::numeric_limits<double>::epsilon())
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "Grid.Col.KCenter: " << sicd.grid->col->kCenter << std::endl
                << "Center frequency * 2/c: " << kfc;
            mLog->error(messageBuilder.str());
            valid = false;
        }
    }

    //2.12.1.10
    if (!Init::isUndefined(sicd.grid->col->deltaKCOAPoly) && sicd.grid->col->deltaKCOAPoly.orderX() > 1)
    {
        messageBuilder.str("");
        messageBuilder << "Grid.Col.DetlaKCOAPoly must be a single value for RGAZCOMP data";
        mLog->error(messageBuilder.str());
        valid = false;
    }

    //2.12.1.11
    if (!Init::isUndefined(sicd.grid->row->deltaKCOAPoly) && sicd.grid->row->deltaKCOAPoly.orderX() > 1)
    {
        messageBuilder.str("");
        messageBuilder << "Grid.Row.DetlaKCOAPoly must be a single value for RGAZCOMP data";
        mLog->error(messageBuilder.str());
        valid = false;
    } 

    Vector3 rowUnitVector = sicd.grid->row->unitVector;
    Vector3 colUnitVector = sicd.grid->col->unitVector;

    Vector3 scp = sicd.geoData->scp.ecf;
    Vector3 arp = sicd.scpcoa->arpPos;
    Vector3 arpVel = sicd.scpcoa->arpVel;
    Vector3 unitRG = (scp - arp) / (scp - arp).norm();
    Vector3 left = cross((arp / arp.norm()), (arpVel / arp.norm()));
    int look = sign(left.dot(unitRG));
    Vector3 spn = cross(unitRG, arpVel) * -look;
    spn = spn / spn.norm();

    //2.12.1.3
    if (sicd.rgAzComp.get() == NULL)
    {
        messageBuilder.str("");
        messageBuilder << "RGAZCOMP image formation declared, but no RgAzComp metadata given." << std::endl
            << "No RgAzcomp filed in this SICD";
        mLog->error(messageBuilder.str());
        valid = false;
    }
    else
    {
        //2.12.1.4
        double derivedAzSF = -look * std::sin(math::Constants::DEGREES_TO_RADIANS * sicd.scpcoa->dopplerConeAngle) / sicd.scpcoa->slantRange;
        if (std::abs(sicd.rgAzComp->azSF - derivedAzSF) > 1e-6)
        {
            messageBuilder.str("");
            messageBuilder << "RGAZCOMP fields inconsistent." << std::endl
                << "RgAzComp.AzSF: " << sicd.rgAzComp->azSF << std::endl
                << "Derived RgAzComp.AzSF: " << derivedAzSF;
            mLog->error(messageBuilder.str());
            valid = false;
        }
        // 2.12.1.5 omitted, polyval, polyder
        if (sicd.timeline.get() != NULL && sicd.timeline->interPulsePeriod.get() != NULL &&
            sicd.timeline->interPulsePeriod->sets.size() == 1)
        {
            double krgCoa = sicd.grid->row->kCenter;
            
            // TODO: In the MATLAB code, krgCoa gets added to sicd.grid->row->deltaKCOAPoly if it exists
            // I'm not sure that makes sense
            Poly1D interPulsePeriodPoly = sicd.timeline->interPulsePeriod->sets[0].interPulsePeriodPoly;
            double stRateCoa = interPulsePeriodPoly.derivative()(sicd.scpcoa->scpTime);
            double deltaKazPerDeltaV = look * krgCoa * (arpVel.norm() * std::sin(
                     sicd.scpcoa->dopplerConeAngle * math::Constants::DEGREES_TO_RADIANS) / sicd.scpcoa->slantRange) / stRateCoa;
            Poly1D derivedKazPoly = deltaKazPerDeltaV * interPulsePeriodPoly;

            double polyNorm = 0;
            for (size_t ii = 0; ii < std::min(sicd.rgAzComp->kazPoly.size(), derivedKazPoly.size()); ++ii)
            {
                polyNorm += std::pow(sicd.rgAzComp->kazPoly[ii] - derivedKazPoly[ii], 2);
            }
            if (std::sqrt(polyNorm) > 1e-3 || sicd.rgAzComp->kazPoly.order() != derivedKazPoly.order())
            {
                messageBuilder.str("");
                messageBuilder << "RGAZCOMP fields inconsistent." << std::endl
                    << "RgAzComp.KazPoly: " << sicd.rgAzComp->kazPoly << std::endl
                    << "Derived RgAzComp.KazPoly: " << derivedKazPoly;
                mLog->error(messageBuilder.str());
                valid = false;
            }
        }
    }
    //2.12.1.6
    Vector3 drvect = unitRG;
    if ((drvect - rowUnitVector).norm() > UVECT_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "UVect fields inconsistent." << std::endl
            << "Grid.Row.UVectECEF: " << rowUnitVector << std::endl
            << "Derived Grid.Row.UVectECEF: " << drvect;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    //2.12.1.7
    Vector3 dcvect = cross(spn, unitRG);
    if ((dcvect - colUnitVector).norm() > UVECT_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "UVectr fields inconsistent." << std::endl
            << "Grid.Col.UVectECF: " << colUnitVector << std::endl
            << "Derived Grid.Col.UVectECF: " << dcvect;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    return valid;
}

std::vector<double> ComplexXMLValidator::linspace(double start, double end, size_t count) const
{
    std::vector<double> ret;
    double stepSize = (end - start) / (count-1);
    ret[0] = start;
    for (size_t ii = 1; ii < count; ++ii)
    {
        ret[ii] = ret[ii - 1] + stepSize;
    }
    return ret;
}

Poly1D ComplexXMLValidator::polyAt(PolyXYZ poly, size_t idx) const
{
    if (idx > 2)
    {
        throw except::Exception(Ctxt("Idx should be < 3"));
    }

    std::vector<double> coefs;
    for (size_t ii = 0; ii < poly.size(); ++ii)
    {
        coefs.push_back(poly[ii][idx]);
    }
    Poly1D ret(coefs);
    return ret;
}

bool ComplexXMLValidator::checkPFA()
{
    bool valid = true;
    try
    {
        //2.12.2.2
        assertExists(sicd.pfa, "SICD.PFA");
    }
    catch (except::Exception&)
    {
        return false;
    }

    //2.12.2.1
    if (sicd.grid->type != ComplexImageGridType::RGAZIM)
    {
        messageBuilder.str("");
        messageBuilder << "PFA image formation should result in a RGAZIM grid." << std::endl
            << "Grid.Type: " << sicd.grid->type.toString();
        mLog->error(messageBuilder.str());
        valid = false;
    }

    //2.12.2.3
    if (sicd.scpcoa.get() == NULL || sicd.pfa->polarAngleRefTime - sicd.scpcoa->scpTime > std::numeric_limits<double>::epsilon())
    {
        messageBuilder.str("");
        messageBuilder << "Polar angle reference time and center of aperture time for center are usuallly the same." << std::endl
            << "PFA.PolarAngRefTime: " << sicd.pfa->polarAngleRefTime << std::endl
            << "SCPCOA.SCPTime: " << (sicd.scpcoa.get() == NULL ? "NULL" : str::toString(sicd.scpcoa->scpTime));
        mLog->warn(messageBuilder.str());
        valid = false;
    }

    /* This is a start on the next few sections
     * It requires reimplementation of polyfit, bsxfun, and pfa_polar_coords
    size_t numPfaCoefs = sicd.pfa->polarAnglePoly.order();
    std::vector<double> times = linspace(0, sicd.timeline->collectDuration, numPfaCoefs + 1);
    std::vector<std::vector<double> > pos;
    for (size_t ii = 0; ii < 3; ++ii)
    {
        std::vector<double> current;
        Poly1D poly = polyAt(sicd.position->arpPoly, ii);
        for (size_t jj = 0; jj < times.size(); ++jj)
        {
            current.push_back(poly(times[jj]));
        }
        pos.push_back(current);
    }

    std::vector<double> polRefPos;
    for (size_t ii = 0; ii < 3; ++ii)
    {
        Poly1D poly = polyAt(sicd.position->arpPoly, ii);
        polRefPos.push_back(poly(sicd.pfa->polarAngleRefTime));
    }
    */

    // Make sure Row.kCtr is consistent with processed RF frequency bandwidth
    if (sicd.radarCollection.get() == NULL || Init::isUndefined(sicd.radarCollection->refFrequencyIndex))
    {
        if (sicd.imageFormation.get() != NULL)
        {
            double fcProc = (sicd.imageFormation->txFrequencyProcMin + sicd.imageFormation->txFrequencyProcMax) / 2;

            // PFA.SpatialFreqSFPoly affects Row.KCtr
            double kapCtr = fcProc * sicd.pfa->spatialFrequencyScaleFactorPoly[0] * 2 / math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC;
            // PFA inscription could cause kapCtr and Row.KCtr to be somewhat different
            double theta = std::atan((sicd.grid->col->impulseResponseBandwidth / 2) / sicd.grid->row->kCenter); // aperture angle
            double kCtrTol = 1 - std::cos(theta); // Difference between krg and Kap (Krg = cos(theta)*kap)
            kCtrTol = std::max(0.01, kCtrTol); // % .01 should be plenty of tolerance for precision issues at small angles

            if (std::abs(sicd.grid->row->kCenter / kapCtr) - 1 > kCtrTol)
            {
                messageBuilder.str("");
                messageBuilder << WF_INCONSISTENT_STR
                    << "Grid.Row.KCtr: " << sicd.grid->row->kCenter << std::endl
                    << "Derived KapCtr: " << kapCtr;
                mLog->error(messageBuilder.str());
                valid = false;
            }
        }
    }
    return valid;
}

bool ComplexXMLValidator::checkRMAT()
{
    bool valid = true;

    // 2.12.3.2.1
    if (sicd.grid->type != ComplexImageGridType::XCTYAT)
    {
        messageBuilder.str("");
        messageBuilder << "RMA/RMAT image formation should result in a XCTYAT grid." << std::endl
            << "RMA.ImageType: RMAT, Grid.Type: " << sicd.grid->type.toString();
        mLog->error(messageBuilder.str());
        valid = false;
    }

    // 2.12.3.2 has to do with the XML representation

    Vector3 scp = sicd.geoData->scp.ecf;
    Vector3 rowUnitVector = sicd.grid->row->unitVector;
    Vector3 colUnitVector = sicd.grid->col->unitVector;
    Vector3 posRef = sicd.rma->rmat->refPos;
    Vector3 velRef = sicd.rma->rmat->refVel;
    Vector3 uLOS = (scp - posRef) / (scp - posRef).norm();
    Vector3 left = cross(posRef / posRef.norm(), velRef / velRef.norm());
    int look = sign(left.dot(uLOS));
    Vector3 uYAT = (velRef / velRef.norm()) * look;
    Vector3 spn = cross(uLOS, uYAT);
    spn = spn / spn.norm();
    Vector3 uXCT = cross(uYAT, spn);
    double dcaRef = std::acos((velRef / velRef.norm()).dot(uLOS)) * math::Constants::RADIANS_TO_DEGREES;

    // 2.12.3.2.3
    if ((rowUnitVector - uXCT).norm() > UVECT_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "UVect fields inconsistent." << std::endl
            << "Grid.Row.UVectECF: " << rowUnitVector
            << "Derived grid.Row.UVectECT: " << uXCT;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    // 2.12.3.2.4
    if ((colUnitVector - uYAT).norm() > UVECT_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "UVect fields inconsistent." << std::endl
            << "Grid.Col.UVectECF: " << colUnitVector
            << "Derived Grid.Col.UVectECF: " << uYAT;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    // 2.12.3.2.5
    if (std::abs(dcaRef - sicd.rma->rmat->dopConeAngleRef) > 1e-6)
    {
        messageBuilder.str("");
        messageBuilder << "RMA fields inconsistent." << std::endl
            << "RMA.RMAT.DopConeAngleRef: " << sicd.rma->rmat->dopConeAngleRef
            << std::endl << "Derived RMA.RMAT.DopConeAngleRef: " << dcaRef;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    if (sicd.radarCollection.get() == NULL || Init::isUndefined(sicd.radarCollection->refFrequencyIndex))
    {
        double fcProc = (sicd.imageFormation->txFrequencyProcMin + sicd.imageFormation->txFrequencyProcMax) / 2;
        double kfc = fcProc * (2 / math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC);

        // 2.12.3.2.6
        if ((kfc * std::sin(sicd.rma->rmat->dopConeAngleRef * math::Constants::RADIANS_TO_DEGREES) / sicd.grid->row->kCenter) - 1 > WF_TOL)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "Grid.Row.KCtr: " << sicd.grid->row->kCenter << std::endl
                << "Derived Grid.Row.KCtr: " << kfc * std::sin(sicd.rma->rmat->dopConeAngleRef * math::Constants::RADIANS_TO_DEGREES);
            mLog->warn(messageBuilder.str());
            valid = false;
        }

        // 2.12.3.2.7
        if ((kfc * std::cos(sicd.rma->rmat->dopConeAngleRef * math::Constants::DEGREES_TO_RADIANS) / sicd.grid->col->kCenter) - 1 > WF_TOL)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "Grid.Col.KCtr: " << sicd.grid->col->kCenter << std::endl
                << "Derived Grid.Col.KCtr: " << kfc * std::cos(sicd.rma->rmat->dopConeAngleRef * math::Constants::DEGREES_TO_RADIANS);
            mLog->warn(messageBuilder.str());
            valid = false;
        }
    }
    return valid;
}

bool ComplexXMLValidator::checkRMCR()
{
    bool valid = true;

    // 2.12.3.3.1
    if (sicd.grid->type != ComplexImageGridType::XRGYCR)
    {
        messageBuilder.str("");
        messageBuilder << "RMA/RMCR image formation should result in a XRGYCR grid." << std::endl
            << "RMA.ImageType: RMCR, Grid.Type: " << sicd.grid->type.toString();
        mLog->error(messageBuilder.str());
        valid = false;
    }

    // 2.12.3.3.2 deals with the XML representation

    Vector3 scp = sicd.geoData->scp.ecf;
    Vector3 rowUnitVector = sicd.grid->row->unitVector;
    Vector3 colUnitVector = sicd.grid->col->unitVector;
    Vector3 posRef = sicd.rma->rmcr->refPos;
    Vector3 velRef = sicd.rma->rmcr->refVel;
    Vector3 uXRG = (scp - posRef) / (scp - posRef).norm();
    Vector3 left = cross(posRef / posRef.norm(), velRef / velRef.norm());
    int look = sign(left.dot(uXRG));
    Vector3 spn = cross(velRef.unit(), uXRG).unit() * look;
    Vector3 uYCR = cross(spn, uXRG);
    double dcaRef = std::acos(velRef.unit().dot(uXRG)) * math::Constants::RADIANS_TO_DEGREES;

    //2.12.3.3.3
    if ((rowUnitVector - uXRG).norm() > UVECT_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "UVect fields inconsistent." << std::endl
            << "Grid.Row.UVectECF: " << rowUnitVector << std::endl
            << "Derived Grid.Row.UVectECF: " << uXRG;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    // 2.12.3.3.4
    if ((colUnitVector - uYCR).norm() > UVECT_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "UVect fields inconsistent." << std::endl
            << "Grid.Col.UVectECF: " << colUnitVector << std::endl
            << "Derived Grid.Col.UVectECF: " << uYCR;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    // 2.12.3.3.5
    if (std::abs(dcaRef - sicd.rma->rmcr->dopConeAngleRef) > 1e-6)
    {
        messageBuilder.str("");
        messageBuilder << "RMA fields inconsistent." << std::endl
            << "RMA.RMCR.DopConeAngleRef: " << sicd.rma->rmcr->dopConeAngleRef << std::endl
            << "Derived RMA.RMCR.DopConeAngleRef: " << dcaRef;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    // 2.12.3.3.6
    if (sicd.grid->col->kCenter != 0)
    {
        messageBuilder.str("");
        messageBuilder << "Grid.Col.KCtr must be zero for RMA/RMCR data." << std::endl
            << "Grid.Col.KCtr = " << sicd.grid->col->kCenter;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    // 2.12.3.3.7
    if (sicd.radarCollection.get() == NULL || Init::isUndefined(sicd.radarCollection->refFrequencyIndex))
    {
        double fcProc = (sicd.imageFormation->txFrequencyProcMin + sicd.imageFormation->txFrequencyProcMax) / 2;
        double kfc = fcProc * (2 / math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC);
        if ((sicd.grid->row->kCenter / kfc) - 1 > WF_TOL)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR << std::endl
                << "Grid.Row.KCtr: " << sicd.grid->row->kCenter << std::endl
                << "Center frequency * 2/c: " << kfc;
            mLog->warn(messageBuilder.str());
            valid = false;
        }
    }
    return valid;
}

bool ComplexXMLValidator::checkINCA()
{
    
    bool valid = true;

    // 2.12.3.4.1
    if (sicd.grid->type != ComplexImageGridType::RGZERO)
    {
        messageBuilder.str("");
        messageBuilder << "RMA/INCA image formation should result in a RGZERO grid." << std::endl
            << "Grid.Type: " << sicd.grid->type.toString();
        mLog->error(messageBuilder.str());
        valid = false;
    }

    // 2.12.3.4.2 deals with the XML representation

        //2.12.3.4.3
    if (sicd.collectionInformation->radarMode == RadarModeType::SPOTLIGHT &&
        (!Init::isUndefined(sicd.rma->inca->dopplerCentroidPoly) ||
            !Init::isUndefined(sicd.rma->inca->dopplerCentroidCOA)))
    {
        messageBuilder.str("");
        messageBuilder << "RMA.INCA fields inconsistent." << std::endl
            << "RMA.INCA.DopplerCentroidPoly/DopplerCentroidCOA not used for SPOTLIGHT collection.";
        mLog->error(messageBuilder.str());
        valid = false;
    }
    //2.12.3.4.4
    else if (sicd.collectionInformation->radarMode != RadarModeType::SPOTLIGHT)
    {
        if (Init::isUndefined(sicd.rma->inca->dopplerCentroidPoly) || Init::isUndefined(sicd.rma->inca->dopplerCentroidCOA))
        {
            messageBuilder.str("");
            messageBuilder << "RMA.INCA fields inconsistent." << std::endl
                << "RMA.INCA.DopplerCentroidPoly/COA required for non-SPOTLIGHT collection.";
            mLog->error(messageBuilder.str());
            valid = false;
        }
        else
        {
            std::vector<std::vector<double> > difference;
            difference.resize(2);
            for (size_t ii = 0; ii < 2; ++ii)
            {
                Poly2D kcoaPoly = sicd.grid->col->deltaKCOAPoly;
                Poly2D centroidPoly = sicd.rma->inca->dopplerCentroidPoly;
                for (size_t jj = 0; jj < std::min(kcoaPoly.orderY(), centroidPoly.orderY()); ++jj)
                {
                    double val = kcoaPoly[ii][jj] - (centroidPoly[ii][jj] * sicd.rma->inca->timeCAPoly(2));
                    difference[ii].push_back(val);
                }
            }
            double norm = 0;
            for (size_t ii = 0; ii < difference.size(); ++ii)
            {
                for (size_t jj = 0; jj << difference[ii].size(); ++jj)
                {
                    norm += std::pow(difference[ii][jj], 2);
                }
            }
            norm = std::sqrt(norm);

            if (!Init::isUndefined(sicd.grid->col->deltaKCOAPoly) &&
                sicd.rma->inca->dopplerCentroidCOA == BooleanType::IS_TRUE &&
                norm > IFP_POLY_TOL)
            {
                messageBuilder.str("");
                messageBuilder << "RMA.INCA fields inconsistent." << std::endl
                    << "Compare Grid.Col.KCOAPoly to RMA.INCA.DopCentroidPoly * RMA.INCA.TimeCAPoly(2).";
                mLog->error(messageBuilder.str());
                valid = false;
            }
        }
    }

    // INCA UVects are defined from closest approach position/velocity
    // not center of aperture
    std::vector<double> caPos;
    caPos.resize(3);
    for (size_t ii = 0; ii < 3; ++ii)
    {
        Poly1D poly = polyAt(sicd.position->arpPoly, ii);
        caPos[ii] = poly(sicd.rma->inca->timeCAPoly(1));
    }

    std::vector<double> caVel;
    caVel.resize(3);
    for (size_t ii = 0; ii < 3; ++ii)
    {
        Poly1D poly = polyAt(sicd.position->arpPoly, ii).derivative();
        caVel[ii] = poly(sicd.rma->inca->timeCAPoly(1));
    }

    Vector3 scp = sicd.geoData->scp.ecf;
    Vector3 rowUnitVector = sicd.grid->row->unitVector;
    Vector3 colUnitVector = sicd.grid->col->unitVector;
    Vector3 caPosVector(caPos);
    Vector3 caVelVector(caVel);
    Vector3 uRG = (scp - caPosVector) / (scp - caPosVector).norm();
    Vector3 left = cross(caPosVector / caPosVector.norm(), caVelVector / caVelVector.norm());
    int look = sign(left.dot(uRG));
    Vector3 spn = cross(uRG, caVelVector) * -look;
    spn = spn / spn.norm();
    Vector3 uAZ = cross(spn, uRG);

    // 2.12.3.4.6
    if ((uRG - rowUnitVector).norm() > UVECT_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "UVectFields inconsistent" << std::endl
            << "Grid.Row.UVectECF: " << rowUnitVector
            << "Derived Grid.Row.UVectECF: " << uRG;
        mLog->error(messageBuilder.str());
        return false;
    }

    // 2.12.3.4.7
    if ((uAZ - colUnitVector).norm() > UVECT_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "UVectFields inconsistent" << std::endl
            << "Grid.Col.UVectECF: " << colUnitVector
            << "Derived Grid.Col.UVectECF: " << uAZ;
        mLog->error(messageBuilder.str());
    }

    // 2.12.3.4.8
    if (sicd.grid->col->kCenter != 0)
    {
        messageBuilder.str("");
        messageBuilder << "Grid.Col.KCtr  must be zero "
            << "for RMA/INCA data." << std::endl
            << "Grid.Col.KCtr: " << sicd.grid->col->kCenter;
        mLog->error(messageBuilder.str());
    }

    // 2.12.3.4.10
    // I'm not sure I'm transcribing R_CA_SCP -> rangeCA correctly
    if ((caPosVector - scp).norm() - sicd.rma->inca->rangeCA > SCPCOA_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "RMA.INCA fields inconsistent." << std::endl
            << "RMA.INCA.rangeCA: " << sicd.rma->inca->rangeCA
            << "Derived RMA.INCA.rangeCA: " << (caPosVector - scp).norm();
        mLog->error(messageBuilder.str());
    }

    // 2.12.3.4.9
    double fc = (sicd.radarCollection->txFrequencyMin +
        sicd.radarCollection->txFrequencyMax) / 2;
    if ((sicd.rma->inca->freqZero / fc) - 1 > WF_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "RMA.INCA.FreqZero is typically the center transmit frequency" << std::endl
            << "RMA.INCA.FreqZero: " << sicd.rma->inca->freqZero
            << "Center transmit frequency: " << fc;
        mLog->warn(messageBuilder.str());
    }

    // 2.12.3.4.11
    if (Init::isUndefined<int>(sicd.radarCollection->refFrequencyIndex) &&
        std::abs(sicd.grid->row->kCenter - sicd.rma->inca->freqZero * 2 /
            math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC) >
        std::numeric_limits<double>::epsilon())
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR << std::endl
            << "RMA.INCA.FreqZero * 2 / c: " << sicd.rma->inca->freqZero * 2 / math::Constants::SPEED_OF_LIGHT_METERS_PER_SEC
            << "Grid.Row.KCenter: " << sicd.grid->row->kCenter;
        mLog->error(messageBuilder.str());
    }
    return valid;
}

bool ComplexXMLValidator::checkRMA()
{
    //2.12.3.1
    try
    {
        assertExists(sicd.rma, "SICD.RMA");
        assertExists(sicd.geoData, "SICD.GeoData");
        assertExists(sicd.grid, "SICD.Grid");
        assertExists(sicd.grid->col, "SICD.Grid.Col");
        assertExists(sicd.grid->row, "SICD.Grid.Row");
    }
    catch (except::Exception&)
    {
        return false;
    }

    if (sicd.rma->rmat.get() != NULL)
    {
        return checkRMAT();
    }

    else if (sicd.rma->rmcr.get() != NULL)
    {
        return checkRMCR();
    }
        
    else if (sicd.rma->inca.get() != NULL)
    {
        return checkINCA();
    }

    messageBuilder.str("");
    messageBuilder << "Exactly one of RMA->RMAT, RMA->RMCR, RMA->INCA "
        << "must exist.";
    mLog->error(messageBuilder.str());
    return false;
}

bool ComplexXMLValidator::checkValidData()
{
    // 2.11 ValidData
    // Both ImageData.ValidData and GeoData.ValidData are optional, 
    // but are conditionally required upon each other. If one exists,
    // the other must also
    bool imageValidData = false;
    bool geoValidData = false;

    if (sicd.imageData.get() != NULL && !sicd.imageData->validData.empty())
    {
        imageValidData = true;
    }
    if (sicd.geoData.get() != NULL && !sicd.geoData->validData.empty())
    {
        geoValidData = true;
    }
    // 2.11.1
    if (imageValidData && !geoValidData)
    {
        messageBuilder.str("");
        messageBuilder << "ImageData.ValidData/GeoData.ValidData required together." << std::endl
            << "ImageData.ValidData exists, but GeoData.ValidData does not.";
        mLog->error(messageBuilder.str());
        return false;
    }
    // 2.11.2
    if (!imageValidData && geoValidData)
    {
        messageBuilder.str("");
        messageBuilder << "ImageData.ValidData/GeoData.ValidData required together." << std::endl
            << "GeoData.ValidData exists, but ImageData.ValidData does not.";
        mLog->error(messageBuilder.str());
        return false;
    }
    /*
    * As with the corner coordinates, we don't check for precision in the agreement
    * between ImageData.ValidData and GeoData.ValidData here, sinc the spec allows
    * from approximate Lat/Lons, and since it's not possible to know the HAE used
    * to compute Lat/Lon for each corner. One could potentially use the SICD sensor
    * model to check that the image coordinates and the resulting ground coordinates
    * from projection to a flat plane (or constant HAE) roughly agreed with GeoData.
    * ValidData to within a generous tolerance.
    */
    return true;
}

bool ComplexXMLValidator::checkGeoData()
{
    // 2.10 GeoData.SCP
    SCP scp = sicd.geoData->scp;

    scene::LLAToECEFTransform transformer;
    Vector3 ecf2 = transformer.transform(scp.llh);
    double ecf_diff = (scp.ecf - ecf2).norm();

    if (ecf_diff > SCPCOA_TOL)
    {
        messageBuilder.str("");
        messageBuilder << "GeoData.SCP.ECF and GeoData.SCP.LLH not consistent." << std::endl
            << "SICD.GeoData.SCP.ECF - SICD.GeoData.SCP.LLH: "
            << ecf_diff << " (m)" << std::endl;
        mLog->error(messageBuilder.str());
        return false;
    }
    return true;
}

double ComplexXMLValidator::nonZeroDenominator(double denominator)
{
    if (denominator == 0)
    {
        return std::numeric_limits<double>::epsilon();
    }
    return denominator;
}

bool ComplexXMLValidator::checkWaveformDescription()
{
    // 2.8 Waveform description consistency
    bool valid = true;
    double wfMin = std::numeric_limits<double>::infinity();
    double wfMax = -std::numeric_limits<double>::infinity();

    for (size_t ii = 0; ii < sicd.radarCollection->waveform.size(); ++ii)
    {
        if (sicd.radarCollection->waveform[ii].get() != NULL)
        {
            double wfCurrent = sicd.radarCollection->waveform[ii]->txFrequencyStart;
            if (wfCurrent < wfMin)
            {
                wfMin = wfCurrent;
            }

            wfCurrent += sicd.radarCollection->waveform[ii]->txRFBandwidth;
            if (wfCurrent > wfMax)
            {
                wfMax = wfCurrent;
            }
        }
    }

    // 2.8.1
    if (wfMin != std::numeric_limits<double>::infinity())
    {
        if (std::abs((wfMin / nonZeroDenominator(sicd.radarCollection->txFrequencyMin)) - 1) > WF_TOL)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.Waveform.WFParameters.TxFreqStart: "
                << wfMin << std::endl
                << "SICD.RadarCollection.TxFrequency.Min: " << sicd.radarCollection->txFrequencyMin
                << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }
    }

    //2.8.2
    if (wfMax != -std::numeric_limits<double>::infinity())
    {
        if (std::abs((wfMax / nonZeroDenominator(sicd.radarCollection->txFrequencyMax)) - 1) > WF_TOL)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.Waveform.WFParameters.TxFreqStart + TxFRBandwidth: "
                << wfMax << std::endl
                << "SICD.RadarCollection.TxFrequency.Max: " << sicd.radarCollection->txFrequencyMax
                << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }
    }

    for (size_t ii = 0; ii < sicd.radarCollection->waveform.size(); ++ii)
    {
        if (sicd.radarCollection->waveform[ii].get() == NULL)
        {
            continue;
        }
        six::sicd::WaveformParameters wfParam = *sicd.radarCollection->waveform[ii];

        //2.8.3
        if (std::abs((wfParam.txRFBandwidth / nonZeroDenominator((wfParam.txPulseLength * wfParam.txFMRate))) - 1) > WF_TOL)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.Waveform.WFParameters.TxFRBandwidth: "
                << wfParam.txRFBandwidth << std::endl
                << "SICD.RadarCollection.TxFrequency.txFMRate * txPulseLength: "
                << wfParam.txFMRate * wfParam.txPulseLength << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        //2.8.4
        if (wfParam.rcvDemodType.toString() == "CHIRP" && wfParam.rcvFMRate != 0)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.Waveform.WFParameters.RcvDemodType: "
                << wfParam.rcvDemodType.toString() << std::endl
                << "SICD.RadarCollection.Waveform.WFParameters.RcvFMRate: "
                << wfParam.rcvFMRate << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        //2.8.5
        if (wfParam.rcvDemodType.toString() == "STRETCH" &&
            std::abs((wfParam.rcvFMRate / nonZeroDenominator(wfParam.txFMRate)) - 1) > WGT_TOL)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.Waveform.WFParameters.RcvDemodType: "
                << wfParam.rcvDemodType << std::endl
                << "SICD.RadarCollection.Waveform.WFParameters.RcvFMRate: "
                << wfParam.rcvFMRate << std::endl
                << "SICD>RadarCollection.Waveform.WFParameters.TxFMRate: "
                << wfParam.txFMRate << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        //2.8.6
        //Absolute frequencies must be positive
        if (six::Init::isUndefined<int>(sicd.radarCollection->refFrequencyIndex) && sicd.radarCollection->txFrequencyMin <= 0)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.txFrequencyMin: "
                << sicd.radarCollection->txFrequencyMin << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        //2.8.7
        //Absolute frequencies must be positive
        if (six::Init::isUndefined<int>(sicd.radarCollection->refFrequencyIndex) && wfParam.txFrequencyStart <= 0)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.txFreqStart: "
                << wfParam.txFrequencyStart << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        //2.8.8
        //Absolute frequencies must be positive
        if (six::Init::isUndefined<int>(sicd.radarCollection->refFrequencyIndex) && wfParam.rcvFrequencyStart <= 0)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.rcvFreqStart: "
                << wfParam.rcvFrequencyStart << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        //2.8.9
        if (wfParam.txPulseLength > wfParam.rcvWindowLength)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.Waveform.WFParameters.TxPulseLength: "
                << wfParam.txPulseLength << std::endl
                << "SICD.RadarCollection.Waveform.WFPArameters.RcvWindowLength: "
                << wfParam.rcvWindowLength << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        //2.8.10
        if (wfParam.rcvIFBandwidth > wfParam.adcSampleRate)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.Waveform.WFParameters.RcvIFBandwidth: "
                << wfParam.rcvIFBandwidth << std::endl
                << "SICD.RadarCollection.Waveform.WFPArameters.ADCSampleRate: "
                << wfParam.adcSampleRate << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        //2.8.11
        if (wfParam.rcvDemodType.toString() == "CHIRP" && wfParam.txRFBandwidth > wfParam.adcSampleRate)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.Waveform.WFParameters.RcvDemodType: "
                << wfParam.rcvDemodType.toString() << std::endl
                << "SICD.RadarCollection.Waveform.WFParameters.TxRFBandwidth: "
                << wfParam.txRFBandwidth << std::endl
                << "SICD.RadarCollection.Waveform.WFPArameters.ADCSampleRate: "
                << wfParam.adcSampleRate << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        //2.8.12
        double freq_tol = (wfParam.rcvWindowLength - wfParam.txPulseLength) * wfParam.txFMRate;
        if (wfParam.rcvFrequencyStart >= (wfParam.txFrequencyStart + wfParam.txRFBandwidth + freq_tol) ||
            wfParam.rcvFrequencyStart <= wfParam.txFrequencyStart - freq_tol)
        {
            messageBuilder.str("");
            messageBuilder << WF_INCONSISTENT_STR
                << "SICD.RadarCollection.Waveform.WFParameters.RcvFreqStart: "
                << wfParam.rcvFrequencyStart << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }
    }
    return valid;
}

bool ComplexXMLValidator::checkARPPoly()
{
    if (sicd.position->arpPoly.order() < 2)
    {
        messageBuilder.str("");
        messageBuilder << "SICD.Position.ARPPoly should have at least position and velocity terms." << std::endl;
        mLog->error(messageBuilder.str());
        return false;
    }
    return true;
}

bool ComplexXMLValidator::checkFFTSigns()
{
    //2.2. FFT signs in both dimensions almost certainly have to be equal
    FFTSign rowSign = sicd.grid->row->sign;
    FFTSign colSign = sicd.grid->col->sign;

    if (rowSign != colSign)
    {
        messageBuilder.str("");
        messageBuilder << "FFT signs in row and column direction should be the same." << std::endl
            << "Grid.Row.Sgn: " << rowSign.toString() << std::endl
            << "Grid.Col.Sgn: " << colSign.toString() << std::endl;
        mLog->error(messageBuilder.str());
        return false;
    }

    return true;
}

bool ComplexXMLValidator::checkFrequencySupportParameters(const DirectionParameters& direction, const std::string& name)
{
    bool valid = true;
    //2.3.1, 2.3.5
    if (direction.deltaK2 <= direction.deltaK1)
    {
        messageBuilder.str("");
        messageBuilder << boundsErrorMessage << std::endl
            << "SICD.Grid." << name << ".DeltaK1: " << direction.deltaK1 << std::endl
            << "SICD.Grid." << name << ".DetalK2: " << direction.deltaK2 << std::endl;
        mLog->error(messageBuilder.str());
        valid = false;
    }

    else
    {
        // 2.3.2, 2.3.6
        if (direction.deltaK2 > (1 / (2 * direction.sampleSpacing)) + std::numeric_limits<double>::epsilon())
        {
            messageBuilder.str("");
            messageBuilder << boundsErrorMessage << std::endl
                << "0.5/SICD.Grid." << name << ".SampleSpacing: " << 0.5/direction.sampleSpacing << std::endl
                << "SICD.Grid." << name << ".DetalK2: " << direction.deltaK2 << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        // 2.3.3, 2.3.7
        if (direction.deltaK1 < (-1 / (2 * direction.sampleSpacing)) - std::numeric_limits<double>::epsilon())
        {
            messageBuilder.str("");
            messageBuilder << boundsErrorMessage << std::endl
                << "0.5/SICD.Grid." << name << ".SampleSpacing: " << 0.5/direction.sampleSpacing << std::endl
                << "SICD.Grid." << name << ".DetalK1: " << direction.deltaK1 << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }

        // 2.3.4, 2.3.8
        if (direction.impulseResponseBandwidth > (direction.deltaK2 - direction.deltaK1) + std::numeric_limits<double>::epsilon())
        {
            messageBuilder.str("");
            messageBuilder << boundsErrorMessage << std::endl
                << "SICD.Grid." << name << ".impulseResponseBandwidth: " << direction.impulseResponseBandwidth << std::endl
                << "SICD.Grid." << name << ".DeltaK2 - SICD.Grid." << name << ".DeltaK1: "
                << direction.deltaK2 - direction.deltaK1 << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }
    }

    // 2.3.9. Compute our own DeltaK1/K2 and test for consistency with DelaKCOAPoly,
    // ImpRespBW, and SS.  Here, we assume the min and max of DeltaKCOAPoly must be
    // on the vertices of the image, since it is smooth and monotonic in most cases--
    // although in actuality this is not always the case.  To be totally generic, 
    // we would have to search for an interior min and max as well
    std::vector<std::vector<sys::SSize_T> > vertices = calculateImageVertices();
    std::vector<double> deltas = calculateDeltaKs(direction, vertices);

    double minDk = deltas[0];
    double maxDk = deltas[1];

    double DK_TOL = 1e-2;

    //2.3.9.1, 2.3.9.3
    if (std::abs(direction.deltaK1 / minDk - 1) > DK_TOL)
    {
        messageBuilder.str("");
        messageBuilder << boundsErrorMessage << std::endl
            << "SICD.Grid." << name << ".DeltaK1: " << direction.deltaK1 << std::endl
            << "Derived Grid." << name << ".DeltaK1: " << minDk << std::endl;
        mLog->error(messageBuilder.str());
        valid = false;
    }
    //2.3.9.2, 2.3.9.4
    if (std::abs(direction.deltaK2 / maxDk - 1) > DK_TOL)
    {
        messageBuilder.str("");
        messageBuilder << boundsErrorMessage << std::endl
            << "SICD.Grid." << name << ".DeltaK2: " << direction.deltaK2 << std::endl
            << "Derived Grid." << name << ".DeltaK2: " << maxDk << std::endl;
        mLog->error(messageBuilder.str());
        valid = false;
    }
    return valid;
}

std::vector<std::vector<sys::SSize_T> > ComplexXMLValidator::calculateImageVertices()
{
    std::vector<std::vector<sys::SSize_T> > vertices;
    vertices.resize(2);

    if (sicd.imageData.get() != NULL && sicd.imageData->validData.size() != 0)
    {
        //test vertices
        for (size_t ii = 0; ii < sicd.imageData->validData.size(); ++ii)
        {
            vertices[0].push_back(sicd.imageData->validData[ii].col);
        }
        for (size_t ii = 0; ii < sicd.imageData->validData.size(); ++ii)
        {
            vertices[1].push_back(sicd.imageData->validData[ii].row);
        }
    }
    else
    {
        //use edges of full image
        vertices[0].push_back(0);
        vertices[0].push_back(sicd.imageData->validData.size() - 1);
        vertices[0].push_back(sicd.imageData->validData.size() - 1);
        vertices[0].push_back(0);
        vertices[1].push_back(0);
        vertices[1].push_back(0);
        vertices[1].push_back(sicd.imageData->validData.size() - 1);
        vertices[1].push_back(sicd.imageData->validData.size() - 1);
    }
    return vertices;
}


std::vector<double> ComplexXMLValidator::calculateDeltaKs(const DirectionParameters& rowCol, 
        std::vector<std::vector<sys::SSize_T> > vertices)
{
    // The calculations for deltaK1 and deltaK2 are interdependent.
    // Since we have to calculate both to calculate either, we may as well return both.
    double deltaK1 = 0;
    double deltaK2 = 0;

    if (!Init::isUndefined<Poly2D>(rowCol.deltaKCOAPoly))
    {
        deltaK1 = std::numeric_limits<double>::infinity();
        deltaK2 = -std::numeric_limits<double>::infinity();

        for (size_t ii = 0; ii < vertices[0].size(); ++ii)
        {
            double currentDeltaK = rowCol.deltaKCOAPoly.atY((double)vertices[1][ii])((double)vertices[0][ii]);
            deltaK1 = std::min(currentDeltaK, deltaK1);
            deltaK2 = std::max(currentDeltaK, deltaK2);
        }
    }

    deltaK1 -= (rowCol.impulseResponseBandwidth / 2);
    deltaK2 += (sicd.grid->row->impulseResponseBandwidth / 2);

    // Wrapped spectrum
    if (deltaK1 < -(1 / rowCol.sampleSpacing) / 2 || deltaK2 >(1 / rowCol.sampleSpacing) / 2)
    {
        deltaK1 = -(1 / rowCol.sampleSpacing) / 2;
        deltaK2 = -deltaK1;
    }

    std::vector<double> deltaKs;
    deltaKs.resize(2);
    deltaKs[0] = deltaK1;
    deltaKs[1] = deltaK2;
    return deltaKs;
}

bool ComplexXMLValidator::checkFrequencySupportParameters()
{
    return (checkFrequencySupportParameters(*sicd.grid->col, "Col") &&
        checkFrequencySupportParameters(*sicd.grid->row, "Row"));
}

bool ComplexXMLValidator::checkSupportParamsAgainstPFA()
{
    // field is conditional
    if (sicd.pfa.get() == NULL)
    {
        return true;
    }

    bool valid = true;
    const DirectionParameters& row = *sicd.grid->row;
    const DirectionParameters& col = *sicd.grid->col;

    //Slow-time deskew would allow for PFA.Kaz2-PFA.Kaz1>(1/Grid.Col.SS),
    //since Kaz bandwidth is compressed from original polar annulus.
    if (sicd.pfa->slowTimeDeskew.get() == NULL || sicd.pfa->slowTimeDeskew->applied.toString() != "IS_TRUE")
    {
        //2.3.10
        if (sicd.pfa->kaz2 - col.kCenter > (1 / (2 * col.sampleSpacing)) + std::numeric_limits<double>::epsilon())
        {
            messageBuilder.str("");
            messageBuilder << boundsErrorMessage << std::endl
                << "0.5/SICD.Grid.Col.SampleSpacing: " << 0.5 / col.sampleSpacing << std::endl
                << "PFA.Kaz2 - Grid.Col.KCenter: " << sicd.pfa->kaz2 - col.kCenter << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }
        //2.3.11
        if (sicd.pfa->kaz1 - col.kCenter < (-1 / (2 * col.sampleSpacing)) - std::numeric_limits<double>::epsilon())
        {
            messageBuilder.str("");
            messageBuilder << boundsErrorMessage << std::endl
                << "0.5/SICD.Grid.Col.SampleSpacing: " << 0.5 / col.sampleSpacing << std::endl
                << "PFA.Kaz1 - Grid.Col.KCenter: " << sicd.pfa->kaz1 - col.kCenter << std::endl;
            mLog->error(messageBuilder.str());
            valid = false;
        }
    }
    //2.3.12
    if (sicd.pfa->krg2 - row.kCenter > (1 / (2 * sicd.grid->row->sampleSpacing)) + std::numeric_limits<double>::epsilon())
    {
        messageBuilder.str("");
        messageBuilder << boundsErrorMessage << std::endl
            << "0.5/SICD.Grid.Row.SampleSpacing: " << 0.5 / row.sampleSpacing << std::endl
            << "PFA.Krg2 - Grid.Row.KCenter: " << sicd.pfa->krg2 - row.kCenter << std::endl;
        mLog->error(messageBuilder.str());
        valid = false;
    }
    //2.3.13
    if (sicd.pfa->krg1 - row.kCenter < (-1 / (2 * sicd.grid->row->sampleSpacing)) - std::numeric_limits<double>::epsilon())
    {
        messageBuilder.str("");
        messageBuilder << boundsErrorMessage << std::endl
            << "0.5/SICD.Grid.Row.SampleSpacing: " << 0.5 / row.sampleSpacing << std::endl
            << "PFA.Krg1 - Grid.Row.KCenter: " << sicd.pfa->krg1 - row.kCenter << std::endl;
        mLog->error(messageBuilder.str());
        valid = false;
    }
    //2.3.14
    if (col.impulseResponseBandwidth > sicd.pfa->kaz2 - sicd.pfa->kaz1 + std::numeric_limits<double>::epsilon())
    {
        messageBuilder.str("");
        messageBuilder << boundsErrorMessage << std::endl
            << "Grid.Col.ImpulseResponseBandwidth: " << col.impulseResponseBandwidth << std::endl
            << "SICD.PFA.Kaz2 - SICD.PFA.Kaz1: " << sicd.pfa->kaz2 - sicd.pfa->kaz1 << std::endl;
        mLog->error(messageBuilder.str());
        valid = false;
    }
    //2.3.15
    if (row.impulseResponseBandwidth > sicd.pfa->krg2 - sicd.pfa->krg1 + std::numeric_limits<double>::epsilon())
    {
        messageBuilder.str("");
        messageBuilder << boundsErrorMessage << std::endl
            << "Grid.Row.ImpulseResponseBandwidth: " << row.impulseResponseBandwidth << std::endl
            << "SICD.PFA.Krg2 - SICD.PFA.Krg1: " << sicd.pfa->krg2 - sicd.pfa->krg1 << std::endl;
        mLog->error(messageBuilder.str());
        valid = false;
    }
    //2.3.16
    if (col.kCenter != 0 && std::abs(sicd.grid->col->kCenter - (sicd.pfa->kaz1 + sicd.pfa->kaz2) / 2) > 1e-5)
    {
        messageBuilder.str("");
        messageBuilder << boundsErrorMessage << std::endl
            << "Grid.Col.KCenter: " << col.kCenter << std::endl
            << "mean(SICD.PFA.Kaz1, SICD.PFA.Kaz2): " << (sicd.pfa->kaz1 + sicd.pfa->kaz2) / 2 << std::endl;
        mLog->error(messageBuilder.str());
        valid = false;
    }
    return valid;
}

Functor* ComplexXMLValidator::calculateWeightFunction(const DirectionParameters& direction)
{
    Functor* weightFunction = NULL;
    bool useWeightFunction = false;

    if (direction.weightType.get() != NULL)
    {
        WeightType weightType = *direction.weightType;
        std::string windowName(weightType.windowName);
        str::upper(windowName);

        if (windowName == "UNIFORM")
        {
            weightFunction = new Identity();
        }
        else if (windowName == "HAMMING")
        {
            double coef;
            if (weightType.parameters.empty() || weightType.parameters[0].str().empty())
            {
                //A Hamming window is defined in many places as a raised cosine of weight .54,
                //so this is the default. However, some data use a generalized raised cosine and
                //call it HAMMING, so we allow for both uses.
                coef = .54;
            }
            else
            {
                coef = str::toType<double>(weightType.parameters[0].str());
            }

            weightFunction = new RaisedCos(coef);
        }
        else if (windowName == "HANNING")
        {
            weightFunction = new RaisedCos(0.50);
        }
        else if (windowName == "KAISER")
        {
            weightFunction = new Kaiser(str::toType<double>(weightType.parameters[0].str()));
        }
        else
        {
            //TODO: windowName == "TAYLOR"
            useWeightFunction = true;
        }
    }
    else
    {
        useWeightFunction = true;
    }

    if (useWeightFunction)
    {
        if (direction.weights.empty())
        {
            return NULL;
        }
        else
        {
            //TODO: interpft(weightFunction)
        }
    }
    return weightFunction;
}

bool ComplexXMLValidator::checkWeightFunctions(const DirectionParameters& direction, const std::string& name)
{
    const size_t DEFAULT_WGT_SIZE = 512;
    std::vector<double> expectedWeights;
    bool valid = true;
    Functor* weightFunction = NULL;

    // 2.4.1, 2.4.2
    if (direction.weightType.get())
    {
        weightFunction = calculateWeightFunction(direction);

        if (weightFunction == NULL)
        {
            messageBuilder.str("");
            messageBuilder << "Unrecognized weighting description " << std::endl
                << "Grid." << name << ".WeightType.WindowName: " << direction.weightType->windowName << std::endl;
            mLog->warn(messageBuilder.str());
            valid = false;
        }
        else
        {
            if (!direction.weights.empty())
            {
                bool consistentValues = true;

                //Arg doesn't matter. Just checking for Uniform-type Functor
                if ((*weightFunction)(5).empty())
                {
                    double key = direction.weights[0];
                    for (size_t ii = 0; ii < direction.weights.size(); ++ii)
                    {
                        if (key != direction.weights[ii])
                        {
                            consistentValues = false;
                        }
                    }
                }
                else
                {
                    expectedWeights = (*weightFunction)(direction.weights.size());

                    for (size_t ii = 0; ii < direction.weights.size(); ++ii)
                    {
                        if (std::abs(expectedWeights[ii] - direction.weights[ii]) > WGT_TOL)
                        {
                            consistentValues = false;
                            break;
                        }
                    }
                }
                if (!consistentValues)
                {
                    messageBuilder.str("");
                    messageBuilder << name << ".weights values inconsistent with " << name << ".weightType" << std::endl
                        << "Grid." << name << ".WeightType.WindowName: " << direction.weightType->windowName << std::endl;
                    mLog->warn(messageBuilder.str());
                    return false;
                }
            }
            else
            {
                expectedWeights = (*weightFunction)(DEFAULT_WGT_SIZE);
            }
        }
    }

    // 2.4.3, 2.4.4
    if (direction.weightType.get() != NULL && direction.weightType->windowName != "UNIFORM" &&
            direction.weightType->windowName != "UNKNOWN" && direction.weights.empty())
    {
        messageBuilder.str("");
        messageBuilder << "Non-uniform weighting, but no WgtFunct provided" << std::endl
            << "SICD.Grid." << name << ".WgtType.WindowName: " << direction.weightType->windowName
            << std::endl;
        mLog->warn(messageBuilder.str());
    }

    // TODO: 2.5 (requires fzero)

    if (weightFunction)
    {
        free(weightFunction);
    }

    return false;
}

bool ComplexXMLValidator::checkWeightFunctions()
{
    DirectionParameters row = *sicd.grid->row;
    DirectionParameters col = *sicd.grid->col;

    bool valid = true;
    // 2.4 Does WgtFunct agree with WgtType?

    // 2.4.1, 2.4.3
    if (row.weightType.get() != NULL)
    {
         valid = valid && checkWeightFunctions(row, "Row");
    }

    // 2.4.2, 2.4.4
    if (col.weightType.get() != NULL)
    {
        valid = valid && checkWeightFunctions(col, "Col");
    }

    return valid;
}

}
}