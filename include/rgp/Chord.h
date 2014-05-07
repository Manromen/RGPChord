/*
 Chord.h
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

#ifndef __RGP__Chord__Chord__
#define __RGP__Chord__Chord__

#include <iostream>

#include <list>
#include <map>
#include <thread>
#include <memory>

#include <rgp/Chord>

namespace rgp {
    
    // forward declaration
    class ChordNode;
    
    class Chord : std::enable_shared_from_this<Chord>
    {
        
    public:
        Chord (std::string ipAddress, uint16_t port);
        Chord (std::string ipAddress, uint16_t port, std::string c_ipAddress, uint16_t c_port);
        ~Chord ();
        
        // key lenght (exponent m of the formular)
        static const int kKeyLenght { 32 };
        
        // helper to quickly create a Chord Header
        ChordHeader createChordHeader (ChordMessageType);
        
        // highest possible hash id
        ChordId highestID () const;
        
        // check if i'm responsible for the given key
        bool keyIsInMyRange (ChordId key) const;
        
        // perform a search for the given key
        ChordHeaderNode searchForKey (ChordId searchingNode, ChordId key) const;
        
        // update predecessor if needed
        ChordHeaderNode updatePredecessor (ChordHeaderNode node);
        
        // searches all data from chord for given node id
        // returns nullptr if not found
        std::shared_ptr<ChordNode> findNodeWithId (ChordId nodeId);
        
        // adds data from remote node to local data map - if we are responsible
        // returns true on success
        // returns false if we aren't responsible
        bool addDataToHashMap (std::shared_ptr<uint8_t> data);
        
        // searches local data for data id and returns the data
        // returns nullptr if there is no data with that key
        std::shared_ptr<uint8_t> getDataWithKey (ChordId dataId);
        
    private:
        // will be initialised from constructor
        std::shared_ptr<ChordNode> _ownNode { nullptr };
        std::shared_ptr<ChordNode> _successor { nullptr };
        std::shared_ptr<ChordNode> _predecessor { nullptr };
        std::list<std::shared_ptr<ChordNode>> _fingerTable;
        
        // table of all local data (the data this node is responsible for)
        std::map<int, std::shared_ptr<uint8_t>> _dataMap;
        
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
        ChordRange _responsibilityRange { .from = 0, .to = 0 };
        
        // method of connectThread
        void waitForIncommingConnections ();
        // my IP-Address and my Port
        void initOwnNode (std::string ipAddress, uint16_t port);
        // join existing DHT using given ip and port
        void joinDHT (std::string c_ipAddress, uint16_t c_port);
        // sets the given node as predecessor
        inline void setPredecessor (ChordHeaderNode node);
        
        // stabilize protocol
        void stabilize ();
        // fix fingers protocol
        void fixFingers ();
    };
}

#endif /* defined(__RGP__Chord__Chord__) */
