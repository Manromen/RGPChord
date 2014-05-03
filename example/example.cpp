/*
 example.cpp
 Chord

 Created by Ralph-Gordon Paul on 13.06.13.

 -------------------------------------------------------------------------------
 The MIT License (MIT)
 
 Copyright (c) 2013 Ralph-Gordon Paul. All rights reserved.
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
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

