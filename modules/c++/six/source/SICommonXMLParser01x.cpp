/* =========================================================================
 * This file is part of six-c++
 * =========================================================================
 *
 * (C) Copyright 2004 - 2013, General Dynamics - Advanced Information Systems
 *
 * six-c++ is free software; you can redistribute it and/or modify
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
#include <set>

#include <six/Utilities.h>
#include <six/SICommonXMLParser01x.h>

namespace
{
typedef xml::lite::Element* XMLElem;
}

namespace six
{

SICommonXMLParser01x::SICommonXMLParser01x(
    const std::string& defaultURI,
    bool addClassAttributes,
    const std::string& siCommonURI,
    logging::Logger* log,
    bool ownLog) :
    SICommonXMLParser(defaultURI, addClassAttributes, 
                      siCommonURI, log, ownLog)
{
}

XMLElem SICommonXMLParser01x::convertCompositeSCPToXML(
    const ErrorStatistics* errorStatistics,
    XMLElem errorStatsXML) const
{
    std::string defaultURI = getSICommonURI();

    //TODO compositeSCP needs to be reworked
    if (errorStatistics->compositeSCP.get())
    {
        XMLElem scpXML = newElement("CompositeSCP", defaultURI, errorStatsXML);

        if (errorStatistics->compositeSCP->scpType == CompositeSCP::RG_AZ)
        {
            XMLElem rgAzXML = newElement("RgAzErr", defaultURI, scpXML);
            createDouble("Rg", defaultURI, errorStatistics->compositeSCP->xErr, rgAzXML);
            createDouble("Az", defaultURI, errorStatistics->compositeSCP->yErr, rgAzXML);
            createDouble("RgAz", defaultURI, errorStatistics->compositeSCP->xyErr,
                         rgAzXML);
        }
        else
        {
            XMLElem rgAzXML = newElement("RowColErr", defaultURI, scpXML);
            createDouble("Row", defaultURI, errorStatistics->compositeSCP->xErr,
                         rgAzXML);
            createDouble("Col", defaultURI, errorStatistics->compositeSCP->yErr,
                         rgAzXML);
            createDouble("RowCol", defaultURI, errorStatistics->compositeSCP->xyErr,
                         rgAzXML);
        }
        return scpXML;
    }
    return NULL;
}

void SICommonXMLParser01x::parseCompositeSCPFromXML(
    const XMLElem errorStatsXML,
    ErrorStatistics* errorStatistics) const
{
    //! optional field
    XMLElem compositeSCPXML = getOptional(errorStatsXML, "CompositeSCP");

    //See if it's RgAzErr or RowColErr
    //optional
    if (compositeSCPXML)
    {
        errorStatistics->compositeSCP.reset(new CompositeSCP());
        XMLElem rgAzErrXML = getOptional(compositeSCPXML, "RgAzErr");
        if (rgAzErrXML)
        {
            errorStatistics->compositeSCP->scpType = CompositeSCP::RG_AZ;
            parseDouble(getFirstAndOnly(rgAzErrXML, "Rg"),
                        errorStatistics->compositeSCP->xErr);
            parseDouble(getFirstAndOnly(rgAzErrXML, "Az"),
                        errorStatistics->compositeSCP->yErr);
            parseDouble(getFirstAndOnly(rgAzErrXML, "RgAz"),
                        errorStatistics->compositeSCP->xyErr);
        }
        else
        {
            XMLElem rowColErrXML = getOptional(compositeSCPXML, "RowColErr");
            if (rowColErrXML)
            {
                errorStatistics->compositeSCP->scpType = CompositeSCP::ROW_COL;
                parseDouble(getFirstAndOnly(rowColErrXML, "Row"),
                            errorStatistics->compositeSCP->xErr);
                parseDouble(getFirstAndOnly(rowColErrXML, "Col"),
                            errorStatistics->compositeSCP->yErr);
                parseDouble(getFirstAndOnly(rowColErrXML, "RowCol"),
                            errorStatistics->compositeSCP->xyErr);
            }
        }
    }
}

XMLElem SICommonXMLParser01x::convertRadiometryToXML(
    const Radiometric *r, XMLElem parent) const
{
    std::string defaultURI = getSICommonURI();
    XMLElem rXML = newElement("Radiometric", getDefaultURI(), parent);

    if (!r->noiseLevel.noisePoly.empty())
    {
        createPoly2D("NoisePoly", defaultURI, r->noiseLevel.noisePoly, rXML);
    }

    if (!r->rcsSFPoly.empty())
    {
        createPoly2D("RCSSFPoly", defaultURI, r->rcsSFPoly, rXML);
    }

    if (!r->betaZeroSFPoly.empty())
    {
        createPoly2D("BetaZeroSFPoly", defaultURI, r->betaZeroSFPoly, rXML);
    }

    if (!r->sigmaZeroSFPoly.empty())
    {
        createPoly2D("SigmaZeroSFPoly",
                     defaultURI,
                     r->sigmaZeroSFPoly,
                     rXML);
    }

    if (r->sigmaZeroSFIncidenceMap != AppliedType::NOT_SET)
    {
        createString(
            "SigmaZeroSFIncidenceMap",
            defaultURI,
            six::toString<six::AppliedType>(r->sigmaZeroSFIncidenceMap),
            rXML);
    }

    if (!r->gammaZeroSFPoly.empty())
    {
        createPoly2D("GammaZeroSFPoly", defaultURI, r->gammaZeroSFPoly, rXML);
    }

    if (r->gammaZeroSFIncidenceMap != AppliedType::NOT_SET)
    {
        createString(
            "GammaZeroSFIncidenceMap",
            defaultURI,
            six::toString<six::AppliedType>(r->gammaZeroSFIncidenceMap),
            rXML);
    }
    return rXML;
}

void SICommonXMLParser01x::parseRadiometryFromXML(
    const XMLElem radiometricXML, 
    Radiometric* radiometric) const
{
    XMLElem tmpElem = NULL;

    tmpElem = getOptional(radiometricXML, "NoisePoly");
    if (tmpElem)
    {
        //optional
        parsePoly2D(tmpElem, radiometric->noiseLevel.noisePoly);
    }

    tmpElem = getOptional(radiometricXML, "RCSSFPoly");
    if (tmpElem)
    {
        //optional
        parsePoly2D(tmpElem, radiometric->rcsSFPoly);
    }

    tmpElem = getOptional(radiometricXML, "BetaZeroSFPoly");
    if (tmpElem)
    {
        //optional
        parsePoly2D(tmpElem, radiometric->betaZeroSFPoly);
    }

    tmpElem = getOptional(radiometricXML, "SigmaZeroSFPoly");
    if (tmpElem)
    {
        //optional
        parsePoly2D(tmpElem, radiometric->sigmaZeroSFPoly);
    }

    tmpElem = getOptional(radiometricXML, "SigmaZeroSFIncidenceMap");
    if (tmpElem)
    {
        //optional
        radiometric->sigmaZeroSFIncidenceMap = 
            six::toType<six::AppliedType>(tmpElem->getCharacterData());
    }

    tmpElem = getOptional(radiometricXML, "GammaZeroSFPoly");
    if (tmpElem)
    {
        //optional
        parsePoly2D(tmpElem, radiometric->gammaZeroSFPoly);
    }

    tmpElem = getOptional(radiometricXML, "GammaZeroSFIncidenceMap");
    if (tmpElem)
    {
        //optional
        radiometric->gammaZeroSFIncidenceMap = 
            six::toType<six::AppliedType>(tmpElem->getCharacterData());
    }
}

}

