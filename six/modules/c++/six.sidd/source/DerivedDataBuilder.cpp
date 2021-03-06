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
#include "six/sidd/DerivedDataBuilder.h"

using namespace six;
using namespace six::sidd;

DerivedDataBuilder::DerivedDataBuilder() :
    mData(new DerivedData()), mAdopt(true)
{
}

DerivedDataBuilder::DerivedDataBuilder(DerivedData *data) :
    mData(data), mAdopt(false)
{
}

DerivedDataBuilder::~DerivedDataBuilder()
{
    if (mData && mAdopt)
        delete mData;
}

DerivedDataBuilder& DerivedDataBuilder::addDisplay(PixelType pixelType)
{
    mData->display.reset(new Display);
    mData->display->pixelType = pixelType;
    return *this;
}

DerivedDataBuilder& DerivedDataBuilder::addGeographicAndTarget(
                                                               RegionType regionType)
{
    mData->geographicAndTarget.reset(new GeographicAndTarget(regionType));

    return *this;
}

DerivedDataBuilder& DerivedDataBuilder::addMeasurement(
                                                       ProjectionType projectionType)
{
    mData->measurement.reset(new Measurement(projectionType));

    return *this;
}

DerivedDataBuilder& DerivedDataBuilder::addExploitationFeatures(
                                                                size_t num)
{
    mData->exploitationFeatures.reset(new ExploitationFeatures(num));

    return *this;
}

DerivedDataBuilder& DerivedDataBuilder::addProductProcessing()
{
    mData->productProcessing.reset(new ProductProcessing());

    return *this;
}

DerivedDataBuilder& DerivedDataBuilder::addDownstreamReprocessing()
{
    mData->downstreamReprocessing.reset(new DownstreamReprocessing());

    return *this;
}

DerivedDataBuilder& DerivedDataBuilder::addErrorStatistics()
{
    mData->errorStatistics.reset(new ErrorStatistics());

    return *this;
}

DerivedDataBuilder& DerivedDataBuilder::addRadiometric()
{
    mData->radiometric.reset(new Radiometric());

    return *this;
}

DerivedData* DerivedDataBuilder::get()
{
    return mData;
}

DerivedData* DerivedDataBuilder::steal()
{
    mAdopt = false;
    return get();
}

