/*
 Chord.h
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

#ifndef __RGP__Chord__
#define __RGP__Chord__

#include <iostream>
#include "ChordNode.h"
#include "ChordData.h"
#include <atomic>
#include <list>
#include <map>
#include <thread>
#include <mutex>

namespace rgp {
    
    // a Chord Data Range (used to identify responsible range)
    class ChordDataRange {
        
    private:
        // properties
        DataID_t from;
        DataID_t to;
        
    public:
        // getter
        DataID_t getFrom() const { return this->from; }
        DataID_t getTo() const { return this->to; }
        
        // setter
        void setFrom (DataID_t from) { this->from = from; }
        void setTo (DataID_t to) { this->to = to; }
        
        // constructor
        ChordDataRange(DataID_t from, DataID_t to) {
            this->from = from;
            this->to = to;
        }
    };
    
    class Chord {
        
    public:
        // Constructor
        Chord (std::string ipAddress, uint16_t port);            // my IP-Address and my Port
        Chord (std::string, uint16_t, std::string, uint16_t);    // my ipAddress, my Port, remote IP-Address and remote Port - to connect to dht
        // Destructor
        ~Chord ();
        
        // attributes
        static const int kKeyLenght { 16 };     // key lenght (exponent m of the formular)
        
        // methods
        void join();                                        // helper for daemon process - main thread will block here
        ChordHeader_t *createChordHeader (ChordMessageType); // helper to quickly create a Chord Header (returned value needs to be deleted)
        
        int highestID () const;                              // highest possible hash id
        bool keyIsInMyRange (DataID_t key) const;            // check if i'm responsible for the given key
        ChordNode_t searchForKey (ChordNode *searchingNode, DataID_t key) const;  // perform a search for the given key
        ChordNode_t updatePredecessor (ChordNode_t node);    // update predecessor if needed
        
        ChordNode *findNodeWithID (NodeID_t nodeID);         // searches all data from chord for given node id (returns nullptr if not found)
        
        // adds data from remote node to local data map - if we are responsible
        // returns true on success
        // returns false if we aren't responsible
        bool addDataToHashMap (std::string data);
        
        // searches local data for data id and returns the data
        // returns nullptr if there is no data with that key
        ChordData *getDataWithKey (DataID_t dataID);
        
        // User calls
        void addData (std::string);          // add data to chord
        void printDataWithHash (DataID_t);   // prints the data for a given key
        void printAllLocalData ();           // print all data from dataMap to stdout
        void printStatus ();                 // print some status data (successor, predecessor, ...)
        
    private:
        // attributes
        ChordNode *ownNode { nullptr };     // will be initialised from constructor
        ChordNode *successor { nullptr };   // successor
        ChordNode *predecessor { nullptr }; // predecessor
        std::list<ChordNode *> fingerTable; // fingle table
        
        std::map<int, ChordData *> dataMap;             // table of all local data (the data this node is responsible for)
        std::mutex dataMap_mutex;                       // protect dataMap
        std::thread connectThread;                      // thread to which unknown nodes can connect to
        std::atomic<bool> stopConnectThread { false };  // set this true to close connectThread
        
        std::thread stabilizeThread;                    // thread for the stabilization protocol
        std::atomic<bool> stopStabilizeThread { false };// stops the stabilization thread
        
        std::list<ChordNode *> connectedNodes;          // all currently connected nodes
        std::mutex connectedNodes_mutex;                // secure access to connectedNodes list
        ChordDataRange responsibilityRange { 0, 0 };    // range that i'm responsible for
        
        // methods
        void initOwnNode (std::string ipAddress, uint16_t port);     // my IP-Address and my Port
        void waitForIncommingConnections ();                         // method of connectThread
        void joinDHT (std::string c_ipAddress, uint16_t c_port);     // join existing DHT using given ip and port
        inline void setPredecessor (ChordNode_t node);               // sets the given node as predecessor
        
        void stabilize ();       // stabilize protocol
        void fix_fingers ();     // fix fingers protocol
    };
}

#endif /* defined(__RGP__Chord__) */
