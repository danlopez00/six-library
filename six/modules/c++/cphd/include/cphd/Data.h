/* =========================================================================
 * This file is part of cphd-c++
 * =========================================================================
 *
 * (C) Copyright 2004 - 2014, MDA Information Systems LLC
 *
 * cphd-c++ is free software; you can redistribute it and/or modify
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


#ifndef __CPHD_DATA_H__
#define __CPHD_DATA_H__

#include <ostream>
#include <vector>
#include <stddef.h>

#include <cphd/Enums.h>

namespace cphd
{
struct ArraySize
{
    ArraySize(size_t v = 0, size_t s = 0);

    bool operator==(const ArraySize& other) const
    {
        return numVectors == other.numVectors &&
               numSamples == other.numSamples;
    }

    bool operator!=(const ArraySize& other) const
    {
        return !((*this) == other);
    }

    size_t numVectors;
    size_t numSamples;
};

std::ostream& operator<< (std::ostream& os, const ArraySize& d);

struct Data
{
    Data();

    size_t getNumChannels() const
    {
        return numCPHDChannels;
    }

    // 1-based channel number
    size_t getNumVectors(size_t channel) const;

    // 1-based channel number
    size_t getNumSamples(size_t channel) const;

    size_t getNumBytesPerSample() const;

    size_t getNumBytesVBP() const
    {
        return numBytesVBP;
    }

    void setNumBytesVBP(size_t n)
    {
        numBytesVBP = n;
    }

    bool operator==(const Data& other) const
    {
        return sampleType == other.sampleType &&
               numCPHDChannels == other.numCPHDChannels &&
               numBytesVBP == other.numBytesVBP &&
               arraySize == other.arraySize;
    }

    bool operator!=(const Data& other) const
    {
        return !((*this) == other);
    }

    SampleType sampleType;
    size_t numCPHDChannels;
    size_t numBytesVBP;

    std::vector<ArraySize> arraySize;
};

std::ostream& operator<< (std::ostream& os, const Data& d);
}

#endif
