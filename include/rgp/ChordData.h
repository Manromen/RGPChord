/*
 ChordData.h
 Chord

 Created by Ralph-Gordon Paul on 13. July 2013.
 
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
