/*
 example.cpp
 Chord
 
 Created by Ralph-Gordon Paul on 02. May 2014.
 
 -------------------------------------------------------------------------------
 The MIT License (MIT)
 
 Copyright (c) 2014 Ralph-Gordon Paul. All rights reserved.
 
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
