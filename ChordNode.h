/*
 ChordNode.h
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

#ifndef __RGP__ChordNode__
#define __RGP__ChordNode__

#include "ChordData.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>

namespace rgp {
    
    // forward class declaration
    class Chord;
    
    // exceptions
    class ChordConnectionException {
        
    private:
        std::string reason;
        
    public:
        ChordConnectionException(std::string reason) : reason(reason) {}
        std::string what() { return reason; }
    };
    
    // type of node id (typedef to be able to update the size) (if updating, use -> 1, 2 or 4 byte)
    // if updating -> update DataID_t to same value
    typedef uint16_t NodeID_t;
    
    // node data as struct
    // this struct should always contain network byte order (for consistency)
    typedef struct {
        NodeID_t nodeID;    // node id
        uint32_t ip;        // ip-address
        uint16_t port;      // listening port
    } ChordNode_t;
    
    // every message begins with this header
    // this struct should always contain network byte order (for consistency)
    typedef struct {
        ChordNode_t node;   // senders node info
        uint8_t type;       // message type (ChordMessageType)
        uint32_t dataSize;  // size of data that follows (0 if there is no data)
    } ChordHeader_t;
    
    // specify which type of message was send
    enum ChordMessageType {
        ChordMessageTypeIdentify = 1,       // if someone connects he identifies himself with his first package
        
        ChordMessageTypeHeartbeat,          // checks if the node is still alive
        ChordMessageTypeHeartbeatReply,     // answers to hearbeat (i'm still alive!)
        
        ChordMessageTypeSearch,             // search for a hash id
        ChordMessageTypeSearchNodeResponse, // answers search with another node (that is responsible for the key)
        
        ChordMessageTypeDataRequest,        // requests data
        ChordMessageTypeDataAnswer,         // send data
        ChordMessageTypeDataNotFound,       // requested data not found
        
        ChordMessageTypeDataAdd,            // add data
        ChordMessageTypeDataAddFailed,      // answers: data wasn't added
        ChordMessageTypeDataAddSuccess,     // answers: data was added successfully
        
        ChordMessageTypeUpdatePredecessor,  // i'm your predecessor
        ChordMessageTypeTellPredecessor,    // tell me your predecessor
        ChordMessageTypePredecessor         // my predecessor is ... (answer to update/tell predecessor)
    };
    
    // feedback of connect()
    enum ChordConnectionStatus {
        ChordConnectionStatusSuccessfullyConnected = 1,
        ChordConnectionStatusConnectingFailed,
        ChordConnectionStatusAlreadyConnected
    };
    
    class ChordNode {
        
    private:
        NodeID_t nodeID { 0 };
        std::string ipAddress { "" };
        uint16_t port { 0 };
        int receiveSocket { -1 };           // request from the other side are incomming here
        int sendSocket { -1 };              // requests are send with this socket
        
        // make send socket thread-safe
        // (several threads will use this:
        // stabilize() thread
        // TUI thread (for search and add data)
        // receiveHandler of other nodes (for search from other nodes etc.)
        std::mutex sendSocket_mutex;
        
        std::thread requestHandlerThread {};// thread to handle incomming requests
        std::atomic<bool> stopRequestHandlerThread { false }; // determines if the requestHandlerThread should exit
        
        Chord *chord { nullptr }; // associated Chord
        
    private:
        void handleRequests(); // handle incomming data (heartbeat, search, ...)
        
        // sends response to remote node
        // throws ChordConnectionException on error
        void sendResponse(ChordMessageType type, char *data, ssize_t dataSize);
        
        // sends request to remote node
        // throws ChordConnectionException on error
        void sendRequest(ChordMessageType type, char *data, ssize_t dataSize);
        
        // receives response from remote node
        // returns pointer to reveived data or nullptr if there was no received data
        // throws ChordConnectionException on error
        char *recvResponse(ChordMessageType *type, ssize_t *dataSize);
        
    public:
        // Constructor
        ChordNode (NodeID_t, std::string, uint16_t, Chord *);   // Node ID, IP-Address, Port, associated Chord
        // Destructor
        ~ChordNode ();
        
        // getter
        NodeID_t getNodeID() const { return this->nodeID; }
        std::string getIPAddress() const { return this->ipAddress; }
        int getPort() const { return this->port; }
        int getReceiveSocket() const { return this->receiveSocket; }
        int getSendSocket() const { return this->sendSocket; }
        
        // setter
        void setSendSocket(int socket) { this->sendSocket = socket; }
        void setReceiveSocket(int socket); // sets new receive socket and starts listening thread
        
        ChordNode_t chordNode_t() const;                    // returns this node as struct ChordNode_t
        
        bool isAlive();                                     // checks if connection is open
        ChordConnectionStatus establishSendConnection();    // establish a connection to remote node (if not already connected)
        
        // send connection can be close
        // (f.e. we have a new successor and don't need to keep the connection alive anymore)
        void closeSendConnection();
        
        // tell's remote node that i'm his predecessor
        // this method is used by stablilization
        // returns node that the remote node returns
        // throws ChordConnectionException on error
        ChordNode_t getPredecessorFromRemoteNode(ChordNode *ownNode);
        
        // search for a key (or a node)
        // throws ChordConnectionException on error
        ChordNode_t searchForKey(DataID_t key);
        
        // receive data for key
        // returns nullptr if data not found
        // throws ChordConnectionException on error
        ChordData *requestDataForKey(DataID_t key);
        
        // sends the data to the remote node to add it there locally
        // returns true on success
        bool addData(std::string *data);
        
        // returns a describing string of the instance (node id, ip, ...)
        std::string description() const;
    };
}

#endif /* defined(__RGP__ChordNode__) */
