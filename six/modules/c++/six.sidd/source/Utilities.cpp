/* =========================================================================
 * This file is part of six.sidd-c++
 * =========================================================================
 *
 * (C) Copyright 2004 - 2014, MDA Information Systems LLC
 *
 * six.sidd-c++ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; If not,
 * see <http://www.gnu.org/licenses/>.
 *
 */
#include "six/Utilities.h"
#include "six/sidd/Utilities.h"
#include "six/sidd/DerivedXMLControl.h"

namespace
{
double getCenterTime(const six::sidd::DerivedData& derived)
{
    double centerTime;
    if (derived.measurement->projection->isMeasurable())
    {
        const six::sidd::MeasurableProjection* const projection =
            reinterpret_cast<const six::sidd::MeasurableProjection*>(
                derived.measurement->projection.get());

        centerTime = projection->timeCOAPoly(0, 0);
    }
    else
    {
        // we estimate...
        centerTime =
                derived.exploitationFeatures->collections[0]->information->collectionDuration
                        / 2;
    }

    return centerTime;
}

namespace
{
void getErrors(const six::sidd::DerivedData& data,
               scene::Errors& errors)
{
    if (!data.measurement->projection->isMeasurable())
    {
        throw except::Exception(Ctxt("Need measurable projection"));
    }

    const six::sidd::MeasurableProjection* const projection =
            reinterpret_cast<const six::sidd::MeasurableProjection*>(
                    data.measurement->projection.get());

    six::getErrors(data.errorStatistics.get(),
                   types::RgAz<double>(projection->sampleSpacing.row,
                                       projection->sampleSpacing.col),
                   errors);
}
}
}

namespace six
{
namespace sidd
{
scene::SideOfTrack
Utilities::getSideOfTrack(const DerivedData* derived)
{
    const double centerTime = getCenterTime(*derived);

    // compute arpPos and arpVel
    const six::Vector3 arpPos = derived->measurement->arpPoly(centerTime);
    const six::Vector3 arpVel =
        derived->measurement->arpPoly.derivative()(centerTime);
    const six::Vector3 refPt =
        derived->measurement->projection->referencePoint.ecef;

    return scene::SceneGeometry(arpVel, arpPos, refPt).getSideOfTrack();
}

std::auto_ptr<scene::SceneGeometry>
Utilities::getSceneGeometry(const DerivedData* derived)
{
    const double centerTime = getCenterTime(*derived);

    // compute arpPos and arpVel
    six::Vector3 arpPos = derived->measurement->arpPoly(centerTime);
    six::Vector3 arpVel =
            derived->measurement->arpPoly.derivative()(centerTime);
    six::Vector3 refPt = derived->measurement->projection->referencePoint.ecef;

    six::Vector3 rowVec;
    six::Vector3 colVec;

    if (derived->measurement->projection->projectionType
            == six::ProjectionType::POLYNOMIAL)
    {
        const six::sidd::PolynomialProjection* projection =
            reinterpret_cast<const six::sidd::PolynomialProjection*>(
                derived->measurement->projection.get());

        double cR = projection->referencePoint.rowCol.row;
        double cC = projection->referencePoint.rowCol.col;

        scene::LatLonAlt centerLLA;
        centerLLA.setLat(projection->rowColToLat(cR, cC));
        centerLLA.setLon(projection->rowColToLon(cR, cC));
        six::Vector3 centerEcef = scene::Utilities::latLonToECEF(centerLLA);

        scene::LatLonAlt downLLA;
        downLLA.setLat(projection->rowColToLat(cR + 1, cC));
        downLLA.setLon(projection->rowColToLon(cR + 1, cC));
        six::Vector3 downEcef = scene::Utilities::latLonToECEF(downLLA);

        scene::LatLonAlt rightLLA;
        rightLLA.setLat(projection->rowColToLat(cR, cC + 1));
        rightLLA.setLon(projection->rowColToLon(cR, cC + 1));
        six::Vector3 rightEcef = scene::Utilities::latLonToECEF(rightLLA);

        rowVec = downEcef - centerEcef;
        rowVec.normalize();
        colVec = rightEcef - centerEcef;
        colVec.normalize();
    }
    else if (derived->measurement->projection->projectionType
            == six::ProjectionType::PLANE)
    {
        const six::sidd::PlaneProjection* projection =
                reinterpret_cast<const six::sidd::PlaneProjection*>(
                    derived->measurement->projection.get());

        rowVec = projection->productPlane.rowUnitVector;
        colVec = projection->productPlane.colUnitVector;
    }
    else if (derived->measurement->projection->projectionType
            == six::ProjectionType::GEOGRAPHIC)
    {
        // In this case there are no image plane row/col vectors, so we want
        // to use a different constructor
        std::auto_ptr<scene::SceneGeometry> geom(new scene::SceneGeometry(
                    arpVel, arpPos, refPt));
        return geom;
    }
    else
    {
        throw except::Exception(Ctxt(
                "Cylindrical projection not yet supported"));
    }

    std::auto_ptr<scene::SceneGeometry> geom(new scene::SceneGeometry(
            arpVel, arpPos, refPt, rowVec, colVec));
    return geom;
}

std::auto_ptr<scene::GridECEFTransform>
Utilities::getGridECEFTransform(const DerivedData* derived)
{
    if (!derived->measurement->projection->isMeasurable())
    {
        throw except::Exception(Ctxt("Projection type is not measurable: " +
                derived->measurement->projection->projectionType.toString()));
    }

    const six::sidd::MeasurableProjection* p =
            reinterpret_cast<const six::sidd::MeasurableProjection*>(
                    derived->measurement->projection.get());

    std::auto_ptr<scene::GridECEFTransform> transform;

    switch ((int) p->projectionType)
    {
    case six::ProjectionType::PLANE:
    {
        const six::sidd::PlaneProjection* const planeP =
                reinterpret_cast<const six::sidd::PlaneProjection*>(p);

        transform.reset(new scene::PlanarGridECEFTransform(
                p->sampleSpacing,
                p->referencePoint.rowCol,
                planeP->productPlane.rowUnitVector,
                planeP->productPlane.colUnitVector,
                p->referencePoint.ecef));
        break;
    }

    case six::ProjectionType::GEOGRAPHIC:
/*
        // Not complete or tested yet
        return new scene::GeographicGridECEFTransform(p->sampleSpacing,
                                                 p->referencePoint.rowCol,
                                                 scene::Utilities::ecefToLatLon(p->referencePoint.ecef));
*/

    case six::ProjectionType::CYLINDRICAL:
/*
        // Not complete or tested yet
    {
        six::Vector3 stripmapDir =
                ((six::sidd::CylindricalProjection*)p)->stripmapDirection;
        six::Vector3 ecef = p->referencePoint.ecef;
        scene::ECEFToLLATransform ecefToLLA;
        scene::LatLonAlt lla = ecefToLLA.transform(ecef);

        scene::WGS84EllipsoidModel model;
        double lon = lla.getLonRadians();
        double lat = atan2(ecef[2], pow(1 - model.calculateFlattening(), 2) *
            sqrt(pow(ecef[0], 2) + pow(ecef[1], 2)));

        six::Vector3 e, n, u;
        e[0] = -sin(lon);
        e[1] = cos(lon);
        e[2] = 0;

        n[0] = -sin(lat) * cos(lon);
        n[1] = -sin(lat) * sin(lon);
        n[2] = cos(lat);

        u[0] = cos(lat) * cos(lon);
        u[1] = cos(lat) * sin(lon);
        u[2] = sin(lat);

        double a = atan2(e.dot(stripmapDir), n.dot(stripmapDir));
        six::Vector3 colVec = cos(a) * n + sin(a) * e;
        six::Vector3 rowVec = math::linear::cross(colVec, u);

        return new scene::CylindricalGridECEFTransform(p->sampleSpacing,
                                                  p->referencePoint.rowCol,
                                                  rowVec,
                                                  colVec,
                                                  u,
                                                  p->referencePoint.ecef,
                                                  ((six::sidd::CylindricalProjection*)p)->curvatureRadius);
    }
*/

    default:
        throw except::Exception(Ctxt("Invalid projection type: " +
                p->projectionType.toString()));

    }

    return transform;
}

std::auto_ptr<scene::GridGeometry>
Utilities::getGridGeometry(const DerivedData* derived)
{
    if (!derived->measurement->projection->isMeasurable())
    {
        throw except::Exception(Ctxt("Projection type is not measurable: " +
                derived->measurement->projection->projectionType.toString()));
    }

    const six::sidd::MeasurableProjection* p =
            reinterpret_cast<const six::sidd::MeasurableProjection*>(
                    derived->measurement->projection.get());

    std::auto_ptr<scene::GridGeometry> geom;

    // Only currently have an implementation for PGD
    switch ((int) p->projectionType)
    {
    case six::ProjectionType::PLANE:
    {
        const six::sidd::PlaneProjection* const planeP =
                reinterpret_cast<const six::sidd::PlaneProjection*>(p);

        geom.reset(new scene::PlanarGridGeometry(
                planeP->productPlane.rowUnitVector,
                planeP->productPlane.colUnitVector,
                p->referencePoint.ecef,
                derived->measurement->arpPoly,
                p->timeCOAPoly));
        break;
    }

    default:
        throw except::Exception(Ctxt("Invalid/unsupported projection type: " +
                p->projectionType.toString()));

    }

    return geom;
}

void Utilities::setProductValues(Poly2D timeCOAPoly,
        PolyXYZ arpPoly, ReferencePoint ref, const Vector3* row,
        const Vector3* col, types::RgAz<double>res, Product* product)
{
    const double scpTime = timeCOAPoly(0, 0);

    Vector3 arpPos = arpPoly(scpTime);
    PolyXYZ arpVelPoly = arpPoly.derivative();
    Vector3 arpVel = arpVelPoly(scpTime);

    setProductValues(arpVel, arpPos, ref.ecef, row, col, res, product);
}

void Utilities::setProductValues(Vector3 arpVel, Vector3 arpPos,
        Vector3 refPos, const Vector3* row, const Vector3* col,
        types::RgAz<double>res, Product* product)
{
    const scene::SceneGeometry sceneGeom(arpVel, arpPos, refPos, *row, *col);

    //do some setup of derived data from geometry
    if (product->north == Init::undefined<double>())
    {
        product->north = sceneGeom.getNorthAngle();
    }

    //if (product->resolution
    //    == Init::undefined<RowColDouble>())
    {
        product->resolution = sceneGeom.getGroundResolution(res);
    }
}

void Utilities::setCollectionValues(Poly2D timeCOAPoly,
        PolyXYZ arpPoly, ReferencePoint ref, const Vector3* row,
        const Vector3* col, Collection* collection)
{
    const double scpTime = timeCOAPoly(0, 0);

    Vector3 arpPos = arpPoly(scpTime);
    PolyXYZ arpVelPoly = arpPoly.derivative();
    Vector3 arpVel = arpVelPoly(scpTime);

    setCollectionValues(arpVel, arpPos, ref.ecef, row, col, collection);
}

void Utilities::setCollectionValues(Vector3 arpVel, Vector3 arpPos,
        Vector3 refPos, const Vector3* row, const Vector3* col,
        Collection* collection)
{
    const scene::SceneGeometry sceneGeom(arpVel, arpPos, refPos, *row, *col);

    if (collection->geometry.get() == NULL)
    {
        collection->geometry.reset(new Geometry());
    }
    if (collection->phenomenology.get() == NULL)
    {
        collection->phenomenology.reset(new Phenomenology());
    }

    if (collection->geometry->slope == Init::undefined<double>())
    {
        collection->geometry->slope = sceneGeom.getETPSlopeAngle();
    }
    if (collection->geometry->squint == Init::undefined<double>())
    {
        collection->geometry->squint = sceneGeom.getSquintAngle();
    }
    if (collection->geometry->graze == Init::undefined<double>())
    {
        collection->geometry->graze = sceneGeom.getETPGrazingAngle();
    }
    if (collection->geometry->tilt == Init::undefined<double>())
    {
        collection->geometry->tilt = sceneGeom.getETPTiltAngle();
    }
    if (collection->geometry->azimuth == Init::undefined<double>())
    {
        collection->geometry->azimuth = sceneGeom.getAzimuthAngle();
    }
    if (collection->phenomenology->multiPath == Init::undefined<double>())
    {
        collection->phenomenology->multiPath = sceneGeom.getMultiPathAngle();
    }
    if (collection->phenomenology->groundTrack == Init::undefined<double>())
    {
        collection->phenomenology->groundTrack =
                sceneGeom.getOPGroundTrackAngle();
    }

    if (collection->phenomenology->shadow == Init::undefined<AngleMagnitude>())
    {
        collection->phenomenology->shadow = sceneGeom.getShadow();
    }

    if (collection->phenomenology->layover == Init::undefined<AngleMagnitude>())
    {
        collection->phenomenology->layover = sceneGeom.getLayover();
    }
}

six::PolarizationType _convertDualPolarization(six::DualPolarizationType pol,
        bool useFirst)
{
    switch (pol)
    {
    case six::DualPolarizationType::OTHER:
        return six::PolarizationType::OTHER;
    case six::DualPolarizationType::V_V:
        return six::PolarizationType::V;
    case six::DualPolarizationType::V_H:
        return useFirst ? six::PolarizationType::V : six::PolarizationType::H;
    case six::DualPolarizationType::H_V:
        return useFirst ? six::PolarizationType::H : six::PolarizationType::V;
    case six::DualPolarizationType::H_H:
        return six::PolarizationType::H;
    case six::DualPolarizationType::RHC_RHC:
        return six::PolarizationType::RHC;
    case six::DualPolarizationType::RHC_LHC:
        return useFirst ? six::PolarizationType::RHC
                        : six::PolarizationType::LHC;
    case six::DualPolarizationType::LHC_RHC:
        return useFirst ? six::PolarizationType::LHC
                        : six::PolarizationType::RHC;
    case six::DualPolarizationType::LHC_LHC:
        return six::PolarizationType::LHC;
    case six::DualPolarizationType::UNKNOWN:
        throw except::Exception(Ctxt("DualPolarizationType::UNKNOWN has no corresponding PolarizationType"));
    default:
        return six::PolarizationType::NOT_SET;
    }
}

std::pair<six::PolarizationType, six::PolarizationType> Utilities::convertDualPolarization(
        six::DualPolarizationType pol)
{
    std::pair<six::PolarizationType, six::PolarizationType> pols;
    pols.first = _convertDualPolarization(pol, true);
    pols.second = _convertDualPolarization(pol, false);
    return pols;
}

std::auto_ptr<scene::ProjectionModel>
Utilities::getProjectionModel(const DerivedData* data)
{
    const int lookDir = getSideOfTrack(data);
    scene::Errors errors;
    ::getErrors(*data, errors);

    std::auto_ptr<scene::SceneGeometry> geom(getSceneGeometry(data));

    const six::ProjectionType gridType =
            data->measurement->projection->projectionType;

    std::auto_ptr<scene::ProjectionModel> projModel;
    switch (gridType)
    {
    case six::ProjectionType::PLANE:
    {
        const six::sidd::PlaneProjection* const plane =
                reinterpret_cast<six::sidd::PlaneProjection*>(
                        data->measurement->projection.get());

        projModel.reset(new scene::PlaneProjectionModel(
                geom->getSlantPlaneZ(),
                plane->productPlane.rowUnitVector,
                plane->productPlane.colUnitVector,
                plane->referencePoint.ecef,
                data->measurement->arpPoly,
                plane->timeCOAPoly,
                lookDir,
                errors));
        break;
    }
    case six::ProjectionType::GEOGRAPHIC:
    {
        const six::sidd::MeasurableProjection* const geo =
                reinterpret_cast<six::sidd::MeasurableProjection*>(
                        data->measurement->projection.get());

        projModel.reset(new scene::GeodeticProjectionModel(
                geom->getSlantPlaneZ(),
                geo->referencePoint.ecef,
                data->measurement->arpPoly,
                geo->timeCOAPoly,
                lookDir,
                errors));
        break;
    }
    case six::ProjectionType::POLYNOMIAL:
    case six::ProjectionType::CYLINDRICAL:
    case six::ProjectionType::NOT_SET:
        throw except::Exception(Ctxt("Grid type not supported: " +
                            gridType.toString()));
    default:
        throw except::Exception(Ctxt("Invalid grid type: " +
                        gridType.toString()));
    }

    return projModel;
}


std::auto_ptr<DerivedData> Utilities::parseData(
    ::io::InputStream& xmlStream,
    const std::vector<std::string>& schemaPaths,
    logging::Logger& log)
{
    XMLControlRegistry xmlRegistry;
    xmlRegistry.addCreator(DataType::DERIVED,
        new XMLControlCreatorT<DerivedXMLControl>());

    std::auto_ptr<Data> data(six::parseData(
        xmlRegistry, xmlStream, schemaPaths, log));

    std::auto_ptr<DerivedData> derivedData(reinterpret_cast<DerivedData*>(
        data.release()));

    return derivedData;
}

std::auto_ptr<DerivedData> Utilities::parseDataFromFile(
    const std::string& pathname,
    const std::vector<std::string>& schemaPaths,
    logging::Logger& log)
{
    io::FileInputStream inStream(pathname);
    return parseData(inStream, schemaPaths, log);
}

std::auto_ptr<DerivedData> Utilities::parseDataFromString(
    const std::string& xmlStr,
    const std::vector<std::string>& schemaPaths,
    logging::Logger& log)
{
    io::StringStream inStream;
    inStream.write(xmlStr);
    return parseData(inStream, schemaPaths, log);
}

std::string Utilities::toXMLString(const DerivedData& data,
    const std::vector<std::string>& schemaPaths,
    logging::Logger* logger)
{
    XMLControlRegistry xmlRegistry;
    xmlRegistry.addCreator(DataType::DERIVED,
        new XMLControlCreatorT<DerivedXMLControl>());

    logging::NullLogger nullLogger;
    return ::six::toValidXMLString(&data,
        schemaPaths,
        (logger == NULL) ? &nullLogger : logger,
        &xmlRegistry);
}
}
}
