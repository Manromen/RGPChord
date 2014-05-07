/*
 ChordNode.h
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

#ifndef __RGP__Chord__ChordNode__
#define __RGP__Chord__ChordNode__

#include <iostream>

#include <rgp/Chord>

namespace rgp {
    
    // forward declaration
    class Chord;
    
    class ChordNode {
        
    public:
        // Constructor
        // Node ID, IP-Address, Port, associated Chord
        ChordNode (ChordId node, std::string ip, uint16_t port,
                   std::shared_ptr<Chord> chord);
        // Destructor
        ~ChordNode ();
        
        // getter
        ChordId getNodeID () const { return this->_nodeID; }
        std::string getIPAddress () const { return this->_ipAddress; }
        int getPort () const { return this->_port; }
        int getReceiveSocket () const { return this->_receiveSocket; }
        int getSendSocket () const { return this->_sendSocket; }
        
        // setter
        void setSendSocket (int socket) { this->_sendSocket = socket; }
        // sets new receive socket and starts listening thread
        void setReceiveSocket (int socket);
        
        // returns this node as struct ChordHeaderNode
        ChordHeaderNode chordNode () const;
        
        // checks if connection is open
        bool isAlive ();
        // establish a connection to remote node (if not already connected)
        ChordConnectionStatus establishSendConnection ();
        
        // send connection can be close
        // (f.e. we have a new successor and don't need to keep the connection
        // alive anymore)
        void closeSendConnection ();
        
        // tell's remote node that i'm his predecessor
        // this method is used by stablilization
        // returns node that the remote node returns
        // throws ChordConnectionException on error
        ChordHeaderNode getPredecessorFromRemoteNode
        (std::shared_ptr<ChordNode> ownNode);
        
        // search for a key (or a node)
        // throws ChordConnectionException on error
        ChordHeaderNode searchForKey (ChordId key);
        
        // receive data for key
        // returns nullptr if data not found
        // throws ChordConnectionException on error
        std::shared_ptr<uint8_t> requestDataForKey (ChordId key);
        
        // sends the data to the remote node to add it there locally
        // returns true on success
        bool addData (std::shared_ptr<uint8_t> data);
        
        // returns a describing string of the instance (node id, ip, ...)
        std::string description () const;
        
    private:
        ChordId _nodeID { 0 };
        std::string _ipAddress { "" };
        uint16_t _port { 0 };
        // request from the other side are incomming here
        int _receiveSocket { -1 };
        // requests are send with this socket
        int _sendSocket { -1 };
        
        // make send socket thread-safe
        // (several threads will use this:
        // stabilize() thread
        // TUI thread (for search and add data)
        // receiveHandler of other nodes (for search from other nodes etc.)
        std::mutex _sendSocket_mutex;
        
        // thread to handle incomming requests
        std::thread _requestHandlerThread {};
        // determines if the requestHandlerThread should exit
        std::atomic<bool> _stopRequestHandlerThread { false };
        
        // associated Chord
        std::weak_ptr<Chord> _chord;
        
        // handle incomming data (heartbeat, search, ...)
        void handleRequests ();
        
        // sends response to remote node
        // throws ChordConnectionException on error
        void sendResponse (ChordMessageType type, std::shared_ptr<uint8_t> data,
                           ssize_t dataSize);
        
        // sends request to remote node
        // throws ChordConnectionException on error
        void sendRequest (ChordMessageType type, std::shared_ptr<uint8_t> data,
                          ssize_t dataSize);
        
        // receives response from remote node
        // returns pointer to reveived data or nullptr if
        // there was no received data
        // throws ChordConnectionException on error
        std::shared_ptr<uint8_t> recvResponse (std::shared_ptr<ChordMessageType> type,
                                               std::shared_ptr<ssize_t> dataSize);
    };
}

#endif /* defined(__RGP__Chord__ChordNode__) */
