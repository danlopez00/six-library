/* =========================================================================
 * This file is part of six-c++
 * =========================================================================
 *
 * (C) Copyright 2004 - 2014, MDA Information Systems LLC
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
#ifndef __SIX_NITF_READ_CONTROL_H__
#define __SIX_NITF_READ_CONTROL_H__

#include <map>
#include "six/NITFImageInfo.h"
#include "six/ReadControl.h"
#include "six/ReadControlFactory.h"
#include "six/Adapters.h"
#include <io/SeekableStreams.h>
#include <import/nitf.hpp>
#include <nitf/IOStreamReader.hpp>

namespace six
{

/*!
 *  \class NITFReadControl
 *  \brief Uses NITRO to read in a Sensor-Independent NITF file
 *
 *  This class encapsulates the details of SICD/SIDD segmentation
 *  presenting the caller with a contiguous view of an Image.  It
 *  uses the XML readers to get DES data into the Data model.
 *
 *  This class takes advantage of optimizations in NITRO specific to
 *  pixel-interleaved, or single band data.
 *
 *  This class is not copyable.
 *
 */
class NITFReadControl : public ReadControl
{
public:

    //!  Constructor
    NITFReadControl();

    //!  Destructor
    virtual ~NITFReadControl()
    {
        reset();
    }

    /*!
     *  Read whether a file has COMPLEX or DERIVED data
     *  \param fromFile path to file
     *  \return datatype of file contents
     */
    virtual DataType getDataType(const std::string& fromFile) const;

    /*!
    *  Read whether a Record has COMPLEX or DERIVED data
    *  \param record the Record in question
    *  \return datatype of Record contents
    */
    static
    DataType getDataType(nitf::Record& record);

    /*!
    *  Read whether a DESegment has COMPLEX or DERIVED data
    *  \param segment the DESegment in question
    *  \return datatype of DESegment contents
    */
    static
    DataType getDataType(nitf::DESegment& segment);

    /*!
    *  Determine whether specific attribute outline COMPLEX or DERIVED contents
    *  Interface to allow communication with programs that use different ways
    *  of storing NITF data
    *  \param desid SICD_XML, SIDD_XML, XML_DATA_CONTENT, etc.
    *  \param subheaderLength length of subheader
    *  \param desshsiField Specification identifier
    *  \param treTag tag of TRE (e.g. XML_DATA_CONTENT)
    *  \return datatype
    */
    static
    DataType getDataType(const std::string& desid,
            sys::Uint64_T subheaderLength,
            const std::string& desshsiField,
            const std::string& treTag="");

    /*!
     *  Performs (Basic) validation when a segment is being
     *  read.  This function is able to test that the image
     *  segment in question at least follows some of the rules
     *  that its supposed to.
     */
    void validateSegment(nitf::ImageSubheader subheader,
                         const NITFImageInfo* info);

    using ReadControl::load;

    /*!
     *  Fulfills our obligations to the parent class, using the IOInterface
     *  method provided with the same name
     *  \param fromFile    Input filepath
     *  \param schemaPaths Directories or files of schema locations
     */
    void load(const std::string& fromFile,
              const std::vector<std::string>& schemaPaths);

    /*
     *  \func load
     *  \brief Loads a SICD from an io::SeekableInputStream.
     *
     *  \param ioStream The stream to read from.
     *  \param schemaPaths Directories or files of schema locations.
     */
    void load(io::SeekableInputStream& ioStream,
              const std::vector<std::string>& schemaPaths);

    void load(mem::SharedPtr<nitf::IOInterface> ioInterface);
    void load(mem::SharedPtr<nitf::IOInterface> ioInterface,
              const std::vector<std::string>& schemaPaths);

    virtual UByte* interleaved(Region& region, size_t imageNumber);

    virtual std::string getFileType() const
    {
        return "NITF";
    }

    // Just in case you need it and are willing to cast
    nitf::Record getRecord() const
    {
        return mRecord;
    }

    // Just in case you need it and are willing to cast
    nitf::Reader getReader() const
    {
        return mReader;
    }

protected:
    //! We keep a ref to the reader
    mutable nitf::Reader mReader;

    //! We keep a ref to the record
    nitf::Record mRecord;

    std::vector<NITFImageInfo*> mInfos;

    std::map<std::string, void*> mCompressionOptions;

    /*!
     *  This function grabs the IID out of the NITF file.
     *  If the data is Complex, it follows the following convention.
     *
     *  - Single-segment NITF: IID is 000
     *  - Multi-segment NITF: IID 001-999
     *
     *  If the data is Derived, the conventions are as follows:
     *
     *  - First 3 characters are the image #, second three characters
     *    are the segment #
     *
     *  NOTE: We are using the image # to populate the Data ID.  Therefore
     *  it is only unique when it's Complex data.
     *
     */
    std::pair<size_t, size_t>
    getIndices(nitf::ImageSubheader& subheader) const;

    void addImageClassOptions(nitf::ImageSubheader& s,
            six::Classification& c) const;

    void addDEClassOptions(nitf::DESubheader& s,
                           six::Classification& c) const;

    void addSecurityOptions(nitf::FileSecurity security,
            const std::string& prefix, six::Options& options) const;

    //! Resets the object internals
    void reset();

    //! All pointers populated within the options need
    //  to be cleaned up elsewhere. There is no access
    //  to deallocation in NITFReadControl directly
    virtual void createCompressionOptions(
            std::map<std::string, void*>& )
    {
    }

private:
    // Unimplemented - NITFReadControl is not copyable
    NITFReadControl(const NITFReadControl& other);
    NITFReadControl& operator=(const NITFReadControl& other);

private:
    std::auto_ptr<Legend> findLegend(size_t productNum);

    void readLegendPixelData(nitf::ImageSubheader& subheader,
                             size_t imageSeg,
                             Legend& legend);

    static
    bool isLegend(nitf::ImageSubheader& subheader)
    {
        std::string iCat = subheader.getImageCategory().toString();
        str::trim(iCat);

        return (iCat == "LEG");
    }

    // We need this for one of the load overloadings
    // to prevent data from being deleted prematurely
    // The issue occurs from the explicit destructor of
    // IOControl
    mem::SharedPtr<nitf::IOInterface> mInterface;
};


struct NITFReadControlCreator : public ReadControlCreator
{
    six::ReadControl* newReadControl() const;

    bool supports(const std::string& filename) const;

};


}

#endif

