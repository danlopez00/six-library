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
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; If not,
 * see <http://www.gnu.org/licenses/>.
 *
 */
#include <six/sicd/RadarCollection.h>
#include <six/Utilities.h>

namespace six
{
namespace sicd
{
TxStep::TxStep() :
    waveformIndex(Init::undefined<int>()),
    txPolarization(PolarizationType::NOT_SET)
{
}

TxStep* TxStep::clone() const
{
    return new TxStep(*this);
}

WaveformParameters::WaveformParameters() :
    txPulseLength(Init::undefined<double>()),
    txRFBandwidth(Init::undefined<double>()),
    txFrequencyStart(Init::undefined<double>()),
    txFMRate(Init::undefined<double>()),
    rcvDemodType(DemodType::NOT_SET),
    rcvWindowLength(Init::undefined<double>()),
    adcSampleRate(Init::undefined<double>()),
    rcvIFBandwidth(Init::undefined<double>()),
    rcvFrequencyStart(Init::undefined<double>()),
    rcvFMRate(Init::undefined<double>())
{
}

bool WaveformParameters::operator==(const WaveformParameters& rhs) const
{
    return (txPulseLength == rhs.txPulseLength &&
        txRFBandwidth == rhs.txRFBandwidth &&
        txFrequencyStart == rhs.txFrequencyStart &&
        txFMRate == rhs.txFMRate &&
        rcvDemodType == rhs.rcvDemodType &&
        rcvWindowLength == rhs.rcvWindowLength &&
        adcSampleRate == rhs.adcSampleRate &&
        rcvIFBandwidth == rhs.rcvIFBandwidth &&
        rcvFrequencyStart == rhs.rcvFrequencyStart &&
        rcvFMRate == rhs.rcvFMRate);
}

WaveformParameters* WaveformParameters::clone() const
{
    return new WaveformParameters(*this);
}

void WaveformParameters::fillDerivedFields()
{
    if (rcvDemodType == DemodType::CHIRP &&
        Init::isUndefined<double>(rcvFMRate))
    {
        rcvFMRate = 0;
    }

    if (rcvFMRate == 0 &&
        rcvDemodType == DemodType::NOT_SET)
    {
        rcvDemodType = DemodType::CHIRP;
    }

    if (Init::isUndefined<double>(txRFBandwidth) &&
        !Init::isUndefined<double>(txPulseLength) &&
        !Init::isUndefined<double>(txFMRate))
    {
        txRFBandwidth = txPulseLength * txFMRate;
    }

    if (!Init::isUndefined<double>(txRFBandwidth) &&
        Init::isUndefined<double>(txPulseLength) &&
        !Init::isUndefined<double>(txFMRate))
    {
        txPulseLength = txRFBandwidth / txFMRate;
    }

    if (!Init::isUndefined<double>(txRFBandwidth) &&
        !Init::isUndefined<double>(txPulseLength) &&
        Init::isUndefined<double>(txFMRate))
    {
        txFMRate = txRFBandwidth / txPulseLength;
    }
}

bool WaveformParameters::validate(int refFrequencyIndex,
        logging::Logger& log) const
{
    bool valid = false;
    std::ostringstream messageBuilder;

    //2.8.3
    if (std::abs(txRFBandwidth / (txPulseLength * txFMRate) - 1) > WF_TOL)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.Waveform.WFParameters.TxFRBandwidth: "
            << txRFBandwidth << std::endl
            << "SICD.RadarCollection.TxFrequency.txFMRate * txPulseLength: "
            << txFMRate * txPulseLength << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    //2.8.4
    if (rcvDemodType == DemodType::CHIRP &&
        rcvFMRate != 0)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.Waveform.WFParameters.RcvDemodType: "
            << rcvDemodType.toString() << std::endl
            << "SICD.RadarCollection.Waveform.WFParameters.RcvFMRate: "
            << rcvFMRate << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    //2.8.5
    if (rcvDemodType == DemodType::STRETCH &&
        std::abs(rcvFMRate / txFMRate - 1) > WGT_TOL)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.Waveform.WFParameters.RcvDemodType: "
            << rcvDemodType << std::endl
            << "SICD.RadarCollection.Waveform.WFParameters.RcvFMRate: "
            << rcvFMRate << std::endl
            << "SICD>RadarCollection.Waveform.WFParameters.TxFMRate: "
            << txFMRate << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    //2.8.7
    //Absolute frequencies must be positive
    if (six::Init::isUndefined<int>(refFrequencyIndex) &&
        txFrequencyStart <= 0)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.txFreqStart: "
            << txFrequencyStart << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    //2.8.8
    //Absolute frequencies must be positive
    if (six::Init::isUndefined<int>(refFrequencyIndex) &&
            rcvFrequencyStart <= 0)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.rcvFreqStart: "
            << rcvFrequencyStart << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    //2.8.9
    if (txPulseLength > rcvWindowLength)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.Waveform.WFParameters.TxPulseLength: "
            << txPulseLength << std::endl
            << "SICD.RadarCollection.Waveform.WFPArameters.RcvWindowLength: "
            << rcvWindowLength << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    //2.8.10
    if (rcvIFBandwidth > adcSampleRate)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.Waveform.WFParameters.RcvIFBandwidth: "
            << rcvIFBandwidth << std::endl
            << "SICD.RadarCollection.Waveform.WFPArameters.ADCSampleRate: "
            << adcSampleRate << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    //2.8.11
    if (rcvDemodType == DemodType::CHIRP && txRFBandwidth > adcSampleRate)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.Waveform.WFParameters.RcvDemodType: "
            << rcvDemodType << std::endl
            << "SICD.RadarCollection.Waveform.WFParameters.TxRFBandwidth: "
            << txRFBandwidth << std::endl
            << "SICD.RadarCollection.Waveform.WFPArameters.ADCSampleRate: "
            << adcSampleRate << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    //2.8.12
    double freq_tol = (rcvWindowLength - txPulseLength) * txFMRate;
    if (rcvFrequencyStart >= (txFrequencyStart + txRFBandwidth + freq_tol) ||
        rcvFrequencyStart <= txFrequencyStart - freq_tol)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.Waveform.WFParameters.RcvFreqStart: "
            << rcvFrequencyStart << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    return valid;
}

ChannelParameters::ChannelParameters() :
    txRcvPolarization(DualPolarizationType::NOT_SET),
    rcvAPCIndex(Init::undefined<int>())
{
}

ChannelParameters* ChannelParameters::clone() const
{
    return new ChannelParameters(*this);
}

AreaDirectionParameters::AreaDirectionParameters() :
    unitVector(Init::undefined<Vector3>()),
    spacing(Init::undefined<double>()),
    elements(Init::undefined<size_t>()),
    first(Init::undefined<size_t>())
{
}

AreaDirectionParameters* AreaDirectionParameters::clone() const
{
    return new AreaDirectionParameters(*this);
}

std::ostream& operator<< (std::ostream& os, const AreaDirectionParameters& d)
{
    os << "AreaDirectionParameters::\n"
       << "  unitVector: " << toString(d.unitVector) << "\n"
       << "  spacing   : " << d.spacing << "\n"
       << "  elements  : " << d.elements << "\n"
       << "  first     : " << d.first << "\n";
     return os;
}

Segment::Segment() :
    startLine(Init::undefined<int>()),
    startSample(Init::undefined<int>()),
    endLine(Init::undefined<int>()),
    endSample(Init::undefined<int>()),
    identifier(Init::undefined<std::string>())
{
}

bool Segment::operator==(const Segment& rhs) const
{
    return (startLine == rhs.startLine &&
        startSample == rhs.startSample &&
        endLine == rhs.endLine &&
        endSample == rhs.endSample &&
        identifier == rhs.identifier);
}

Segment* Segment::clone() const
{
    return new Segment(*this);
}

AreaPlane::AreaPlane() :
    referencePoint(Init::undefined<ReferencePoint>()),
    xDirection(new AreaDirectionParameters()),
    yDirection(new AreaDirectionParameters()),
    orientation(OrientationType::NOT_SET)
{
}

AreaPlane* AreaPlane::clone() const
{
    return new AreaPlane(*this);
}

types::RowCol<double> AreaPlane::getAdjustedReferencePoint() const
{
    types::RowCol<double> refPt = referencePoint.rowCol;

    // NOTE: The calculation done by SICD producers appears to be
    //       orpRow = (numRows + 1) / 2.0
    //       This gives you a pixel-centered, 1-based ORP.  We want
    //       pixel-centered 0-based.  More generally than this, we need to
    //       account for the SICD FirstLine/FirstSample offset
    //
    refPt.row -= xDirection->first;
    refPt.col -= yDirection->first;

    return refPt;
}

Area::Area()
{
    const LatLonAlt initial = Init::undefined<LatLonAlt>();
    for (size_t ii = 0; ii < LatLonAltCorners::NUM_CORNERS; ++ii)
    {
        acpCorners.getCorner(ii) = initial;
    }
}

Area* Area::clone() const
{
    return new Area(*this);
}

RadarCollection* RadarCollection::clone() const
{
    return new RadarCollection(*this);
}

bool RadarCollection::operator==(const RadarCollection& rhs) const
{
    return (refFrequencyIndex == rhs.refFrequencyIndex &&
        txFrequencyMin == rhs.txFrequencyMin &&
        txFrequencyMax == rhs.txFrequencyMax &&
        txPolarization == rhs.txPolarization &&
        polarizationHVAnglePoly == rhs.polarizationHVAnglePoly &&
        txSequence == rhs.txSequence &&
        waveform == rhs.waveform &&
        rcvChannels == rhs.rcvChannels &&
        area == rhs.area &&
        parameters == rhs.parameters);
}

double RadarCollection::waveformMax() const
{
    double derivedMax = -std::numeric_limits<double>::infinity();
    for (size_t ii = 0; ii < waveform.size(); ++ii)
    {
        if (waveform[ii].get() != NULL)
        {
            derivedMax = std::max(derivedMax,
                waveform[ii]->txFrequencyStart +
                waveform[ii]->txRFBandwidth);
        }
    }
    return derivedMax;
}

double RadarCollection::waveformMin() const
{
    double derivedMin = std::numeric_limits<double>::infinity();
    for (size_t ii = 0; ii < waveform.size(); ++ii)
    {
        if (waveform[ii].get() != NULL)
        {
            derivedMin = std::min(derivedMin,
                waveform[ii]->txFrequencyStart);
        }
    }
    return derivedMin;
}

void RadarCollection::fillDerivedFields()
{
    // Transmit bandwidth
    if (!waveform.empty())
    {
        if (Init::isUndefined<double>(txFrequencyMin))
        {
            txFrequencyMin = waveformMin();
        }
        if (Init::isUndefined<double>(txFrequencyMax))
        {
            txFrequencyMax = waveformMax();
        }

        for (size_t ii = 0; ii < waveform.size(); ++ii)
        {
            if (waveform[ii].get() != NULL)
            {
                waveform[ii]->fillDerivedFields();
            }
        }
    }

    if (waveform.size() == 1 &&
        waveform[0].get() != NULL)
    {
        if (Init::isUndefined<double>(waveform[0]->txFrequencyStart))
        {
            waveform[0]->txFrequencyStart = txFrequencyMin;
        }
        if (Init::isUndefined<double>(waveform[0]->txRFBandwidth))
        {
            waveform[0]->txRFBandwidth = txFrequencyMax - txFrequencyMin;
        }
    }
}

bool RadarCollection::validate(logging::Logger& log) const
{
    bool valid = true;
    std::ostringstream messageBuilder;

    // 2.8 Waveform description consistency
    double wfMin = waveformMax();
    double wfMax = waveformMin();

    // 2.8.1
    if (wfMin != std::numeric_limits<double>::infinity() &&
        std::abs((wfMin / txFrequencyMin) - 1) > WF_TOL)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.Waveform.WFParameters.TxFreqStart: "
            << wfMin << std::endl
            << "SICD.RadarCollection.TxFrequency.Min: " << txFrequencyMin
            << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    //2.8.2
    if (wfMax != -std::numeric_limits<double>::infinity() &&
        std::abs((wfMax / txFrequencyMax) - 1) > WF_TOL)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.Waveform.WFParameters.TxFreqStart"
            << " + TxFRBandwidth: " << wfMax << std::endl
            << "SICD.RadarCollection.TxFrequency.Max: "
            << txFrequencyMax << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    //2.8.6
    //Absolute frequencies must be positive
    if (six::Init::isUndefined<int>(refFrequencyIndex) && txFrequencyMin <= 0)
    {
        messageBuilder.str("");
        messageBuilder << WF_INCONSISTENT_STR
            << "SICD.RadarCollection.txFrequencyMin: "
            << txFrequencyMin << std::endl;
        log.error(messageBuilder.str());
        valid = false;
    }

    for (size_t ii = 0; ii < waveform.size(); ++ii)
    {
        if (waveform[ii].get())
        {
            valid = valid && waveform[ii]->validate(refFrequencyIndex, log);
        }
    }
    return valid;
}
}
}
