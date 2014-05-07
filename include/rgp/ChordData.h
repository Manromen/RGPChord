/*
 ChordData.h
 Chord

 Created by Ralph-Gordon Paul on 13. July 2013.
 
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

#ifndef __RGP__Chord__ChordData__
#define __RGP__Chord__ChordData__

#include <iostream>

#include <rgp/Chord>

namespace rgp {
    
    /**
     @brief Abstract Class: Data that can be send / received through Chord.
     @details Data that can be send through Chord has to inherit
     from this abstract data class. There are two methods to serialize the
     instance.
     */
    class ChordData {
        
    private:
        
        /**
         @brief Binary data of an object.
         @details Create a binary data that represents Your object.
         The first 4 bytes (uint32_t) are reserved to represent the data size.
         The data size has to be in network byteorder.
         You are responsible for that.
         @return The binary data that represents the data to be send.
         thtough Chord.
         @sa updateWithSerializedData()
         */
        virtual std::shared_ptr<uint8_t> serializedData () const = 0;
        
        /**
         @brief Update or Recreate the object using the given binary data.
         @details Use this method to (re-)create an data object.
         The first 4 bytes (uint32_t) are reserved to represent the data size.
         The data size is in network byteorder.
         @param data The binary data to recreate the object.
         @sa serializedData()
         */
        virtual void updateWithSerializedData (const std::shared_ptr<uint8_t> data) = 0;
        
        friend class ChordNode;
    };
}

#endif /* defined(__RGP__Chord__ChordData__) */
