/*
 example.cpp
 Chord

 Created by Ralph-Gordon Paul on 13.06.13.

 -------------------------------------------------------------------------------
 GNU Lesser General Public License Version 3, 29 June 2007
 
 Copyright (c) 2013 Ralph-Gordon Paul. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public License
 along with this library.
 -------------------------------------------------------------------------------
*/

#include <iostream>
#include <rgp/Chord>
#include <rgp/Log.h>
#include "example.h"

using namespace rgp;

ExampleData::ExampleData(int32_t number, std::string data)
: _dataNumber(number), _dataString(data)
{
}

std::shared_ptr<uint8_t> ExampleData::serializedData () const
{
    // create reference to our string
    const char *str = _dataString.c_str();
    
    // calculate data size
    uint32_t dataSize { sizeof(uint32_t) + sizeof(_dataNumber) + sizeof(str) };
    
    // create data with enough space
    std::shared_ptr<uint8_t> data { new uint8_t[dataSize], std::default_delete<uint8_t[]>() };
    
    // convert data size to network byteorder
    dataSize = htonl(dataSize);
    
    // create pointer to first position on data
    uint8_t *pos = data.get();
    
    // copy the data into the data
    memcpy(pos, &dataSize, sizeof(dataSize));
    pos += sizeof(dataSize);
    memcpy(pos, &_dataNumber, sizeof(_dataNumber));
    pos += sizeof(_dataNumber);
    memcpy(pos, &str, sizeof(str));
    
    return data;
}

void ExampleData::updateWithSerializedData (const std::shared_ptr<uint8_t> data)
{
    if (data) {
        
        // create pointer to first position on data
        uint8_t *pos = data.get();
        
        uint32_t dataSize { 0 };
        
        // get data size from data
        memcpy(&dataSize, pos, sizeof(dataSize));
        pos += sizeof(dataSize);
        
        // convert data size from network byteorder to host byteorder
        dataSize = ntohl(dataSize);
        dataSize -= sizeof(dataSize);
        
        // copy number from data
        if (dataSize >= sizeof(_dataNumber)) {
            memcpy(&_dataNumber, pos, sizeof(_dataNumber));
            pos += sizeof(_dataNumber);
            dataSize -= sizeof(_dataNumber);
        }
        
        // copy the string from the data
        if (dataSize > 0) {
            std::shared_ptr<char> str { new char[dataSize], std::default_delete<char[]>() };
            memcpy(&str, pos, dataSize);
            _dataString = std::string(str.get());
        }
    }
}

std::string ExampleData::description() const
{
    std::string desc { "ExampleData: " };
    
    desc += _dataString;
    desc += std::string(" ");
    desc += std::to_string(_dataNumber);
    
    return desc;
}


int main (int argc, const char **argv)
{
    
    ExampleData *data1 = new ExampleData(50, "Hallo");
    
    ExampleData *data2 = new ExampleData();
    
    Log::sharedLog()->print(std::string("data1: ") + data1->description());
    Log::sharedLog()->print(std::string("data2: ") + data2->description());

    Log::sharedLog()->print("copy data...");
    
    data2->updateWithSerializedData(data1->serializedData());
    
    Log::sharedLog()->print(std::string("data2: ") + data2->description());
    
    return EXIT_SUCCESS;
}

// print usage information to the output (if usage was wrong)
void printUsage (const char *programName)
{
}

