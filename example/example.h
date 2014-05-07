/*
 example.cpp
 Chord
 
 Created by Ralph-Gordon Paul on 02. May 2014.
 
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

#ifndef __RGP__Chord__Example__
#define __RGP__Chord__Example__

using namespace rgp;

class ExampleData : ChordData {
    
public:
    ExampleData() {};
    ExampleData(int32_t number, std::string data);
    
    std::shared_ptr<uint8_t> serializedData () const;
    void updateWithSerializedData (const std::shared_ptr<uint8_t> data);
    
    std::string description() const;
    
private:
    int32_t _dataNumber { 0 };
    std::string _dataString { "" };
};


#endif /* defined(__RGP__Chord__Example__) */
