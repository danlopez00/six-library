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

#include <iostream>
#include <stdexcept>
#include <string>
#include <memory>

#include <cphd/CPHDWriter.h>
#include <types/RowCol.h>
#include <cli/ArgumentParser.h>

int main(int argc, char** argv)
{
    try
    {
        // Parse the command line
        cli::ArgumentParser parser;
        parser.setDescription("Create a sample CPHD file.");
        parser.addArgument("-t --threads",
                           "Specify the number of threads to use",
                           cli::STORE,
                           "threads",
                           "NUM")->setDefault(sys::OS().getNumCPUs());
        parser.addArgument("-c --channels",
                           "Specify the number of channels to write",
                           cli::STORE,
                           "channels",
                           "NUM")->setDefault(1);
        parser.addArgument("--rows",
                           "Specify the number of rows per channel",
                           cli::STORE,
                           "rows",
                           "NUM")->setDefault(128);
        parser.addArgument("--cols",
                           "Specify the number of cols per channel",
                           cli::STORE,
                           "cols",
                           "NUM")->setDefault(128);
        parser.addArgument("-d --domain",
                           "Specify the domain type of the CPHD",
                           cli::STORE, "domain")->setChoices(
                           str::split("FX TOA"))->setDefault("FX");
        parser.addArgument("output", "Output pathname", cli::STORE, "output",
                           "CPHD", 1, 1);
        const std::auto_ptr<cli::Results> options(parser.parse(argc, argv));

        const size_t numChannels = options->get<size_t>("channels");
        const types::RowCol<size_t> dims(options->get<size_t>("rows"),
                                         options->get<size_t>("cols"));
        const std::vector<size_t> numVectors(numChannels, dims.row);

        const cphd::DomainType domainType(
                options->get<std::string>("domain") ==
                        "FX" ? cphd::DomainType::FX : cphd::DomainType::TOA);

        const std::string outPathname(options->get<std::string>("output"));
        const size_t numThreads(options->get<size_t>("threads"));
        const std::vector<std::complex<float> > data(
                dims.area(), std::complex<float>(0.0f, 0.0f));

        cphd::Metadata metadata;

        metadata.data.numCPHDChannels = numChannels;
        for (size_t ii = 0; ii < numChannels; ++ii)
        {
            metadata.data.arraySize.push_back(
                    cphd::ArraySize(dims.row, dims.col));
        }

        //! Must set the sample type (complex<float>)
        metadata.data.sampleType = cphd::SampleType::RE32F_IM32F;

        //! We must have a radar mode set
        metadata.collectionInformation.radarMode = cphd::RadarModeType::SPOTLIGHT;

        //! We must have corners set
        for (size_t ii = 0; ii < six::LatLonAltCorners::NUM_CORNERS; ++ii)
        {
            metadata.global.imageArea.acpCorners.getCorner(ii).setLat(0.0);
            metadata.global.imageArea.acpCorners.getCorner(ii).setLon(0.0);
            metadata.global.imageArea.acpCorners.getCorner(ii).setAlt(0.0);
        }

        //! Add a channel parameter. This will write without this but it will
        //  not read.
        cphd::ChannelParameters param;
        metadata.channel.parameters.push_back(param);

        //! We must set SRP Type
        metadata.srp.srpType = cphd::SRPType::STEPPED;

        //! We must set domain parameters
        metadata.global.domainType = domainType;
        if (domainType == cphd::DomainType::FX)
        {
            metadata.vectorParameters.fxParameters.reset(new cphd::FxParameters());
        }
        else
        {
            metadata.vectorParameters.toaParameters.reset(new cphd::TOAParameters());
        }

        cphd::VBM vbm(numChannels,
                      numVectors,
                      false,
                      false,
                      false,
                      domainType);

        cphd::CPHDWriter writer(metadata, numThreads);
        writer.writeMetadata(outPathname, vbm);
        for (size_t ii = 0; ii < numChannels; ++ii)
        {
            writer.writeCPHDData(&data[0], dims.area());
        }

        std::cout << "Successfully wrote CPHD file: " << outPathname << "\n";

        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    catch (const except::Exception& ex)
    {
        std::cerr << ex.toString() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown exception\n";
        return 1;
    }
}

