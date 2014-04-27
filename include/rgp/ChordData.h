/*
 ChordData.h
 Chord

 Created by Ralph-Gordon Paul on 20.06.13.
 
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
 
#ifndef __RGP__ChordData__
#define __RGP__ChordData__

#include <iostream>

namespace rgp {

    // type of data id (typedef to be able to update the size) (if updating, use -> 1, 2 or 4 byte)
    // if updating -> update NodeID_t to same value
    typedef uint16_t DataID_t;
    
    class ChordData {
        
    private:
        DataID_t dataID { 0 };
        std::string data { "" };
        
    public:
        // Constructor
        ChordData (DataID_t dataID, std::string data); // dataID, data
        
        // Getter
        DataID_t getID () const { return this->dataID; };
        std::string getData () const { return this->data; }
        
        // returns a describing string of the data
        std::string description () const;
    };
}

#endif /* defined(__RGP__ChordData__) */
