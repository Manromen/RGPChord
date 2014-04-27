/*
 Chord.h
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

#ifndef __RGP__Chord__Chord__
#define __RGP__Chord__Chord__

#include <iostream>

#include <list>
#include <map>
#include <thread>
#include <memory>

#include <rgp/ChordTypes.h>
#include <rgp/ChordNode.h>
#include <rgp/ChordData.h>

namespace rgp {
    
    // forward declaration
    class ChordData;
    
    class Chord {
        
    public:
        Chord ();
        ~Chord ();
        
        // key lenght (exponent m of the formular)
        static const int kKeyLenght { 32 };
        
        // helper to quickly create a Chord Header
        std::shared_ptr<ChordHeader> createChordHeader (ChordMessageType);
        
        // highest possible hash id
        int highestID () const;
        
        // check if i'm responsible for the given key
        bool keyIsInMyRange (ChordId key) const;
        
        // perform a search for the given key
        ChordHeaderNode searchForKey (std::shared_ptr<ChordNode> searchingNode,
                                      ChordId key) const;
        
        // update predecessor if needed
        ChordHeaderNode updatePredecessor (ChordHeaderNode node);
        
        // searches all data from chord for given node id
        // returns nullptr if not found
        std::shared_ptr<ChordNode> findNodeWithId (ChordId nodeId);
        
        // adds data from remote node to local data map - if we are responsible
        // returns true on success
        // returns false if we aren't responsible
        bool addDataToHashMap (std::string data);
        
        // searches local data for data id and returns the data
        // returns nullptr if there is no data with that key
        std::shared_ptr<ChordData> getDataWithKey (ChordId dataId);
        
    private:
        // will be initialised from constructor
        std::shared_ptr<ChordNode> _ownNode { nullptr };
        std::shared_ptr<ChordNode> _successor { nullptr };
        std::shared_ptr<ChordNode> _predecessor { nullptr };
        std::list<std::shared_ptr<ChordNode>> _fingerTable;
        
        // table of all local data (the data this node is responsible for)
        std::map<int, std::shared_ptr<ChordData>> _dataMap;
        
        // protect dataMap
        std::mutex _dataMap_mutex;
        // thread to which unknown nodes can connect to
        std::thread _connectThread;
        // set this true to close connectThread
        std::atomic<bool> _stopConnectThread { false };
        
        // thread for the stabilization protocol
        std::thread _stabilizeThread;
        // stops the stabilization thread
        std::atomic<bool> _stopStabilizeThread { false };
        
        // all currently connected nodes
        std::list<std::shared_ptr<ChordNode>> _connectedNodes;
        // safeguard access to connectedNodes list
        std::mutex _connectedNodes_mutex;
        // range that i'm responsible for
        ChordRange responsibilityRange { .from = 0, .to = 0 };
        
        // method of connectThread
        void waitForIncommingConnections ();
        // join existing DHT using given ip and port
        void joinDHT (std::string c_ipAddress, uint16_t c_port);
        // sets the given node as predecessor
        inline void setPredecessor (std::shared_ptr<ChordNode> node);
        
        // stabilize protocol
        void stabilize ();
        // fix fingers protocol
        void fixFingers ();
    };
}

#endif /* defined(__RGP__Chord__Chord__) */
