/*
 ChordNode.cpp
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

#include <rgp/ChordNode.h>
#include <rgp/Log.h>

//#include <sstream>
#include <unistd.h>
//#include <cstring>

// network
#include <arpa/inet.h>
#include <netdb.h>

using namespace rgp;

#pragma mark - Constructor / Destructor

ChordNode::ChordNode (ChordId node, std::string ip, uint16_t port, std::shared_ptr<Chord> chord)
: _nodeID(node), _ipAddress(ip), _port(port), _chord(chord)
{
    
}

ChordNode::~ChordNode ()
{
    // stop request handler
    _stopRequestHandlerThread = true;
    try {
        // wait till request handler finished
        _requestHandlerThread.join();
    } catch (...) {} // called if requestHandlerThread is not running (or not joinable)
    
    // disconnect sendSocket if connected
    if (_sendSocket != -1) {
        close(_sendSocket);
        _sendSocket = -1;
    }
}

#pragma mark - Public

#pragma mark - Private

void ChordNode::setReceiveSocket (int socket)
{
    if (_receiveSocket != -1) {
        Log::sharedLog()->error(std::string("warning receivesocket for node: ") += std::to_string(_nodeID) += " was already set");
    }
    
    // stop request handler if already started
    _stopRequestHandlerThread = true;
    try {
        // wait till request handler finished
        _requestHandlerThread.join();
    } catch (...) {} // called if requestHandlerThread is not running (or not joinable)
    
    _stopRequestHandlerThread = false;
    _receiveSocket = socket;
    
    // start request handler
    if (!_requestHandlerThread.joinable()) { // only if it's not already running
        _requestHandlerThread = std::thread(&ChordNode::handleRequests, this);
    }
}

void ChordNode::handleRequests ()
{
    RGPLOGV("ChordNode handleRequest");
    
    // receive request
    ChordHeader requestHeader { {0, 0, 0}, ChordMessageTypeIdentify, 0 };
    
    std::shared_ptr<uint8_t> data { nullptr };
    ssize_t readBytes { 0 };
    
    while (!_stopRequestHandlerThread) {
        
        data.reset();
        
        // wait for incomming data
        if ((readBytes = recv(_receiveSocket, &requestHeader, sizeof(ChordHeader), 0)) <= 0) {
            if (readBytes == 0) {
                RGPLOGV("Remote Node closed connection");
                break;
            }
            Log::sharedLog()->errorWithErrno("ChordNode::handleRequests():recv1: ", errno);
            break;
        }
        
        // error check
        if (readBytes != sizeof(ChordHeader)) {
            Log::sharedLog()->error("don't received enough data ... something bad happened");
            continue;
        }
        
        // check for available data
        if (ntohl(requestHeader.dataSize) > 0) {
            
            // alloc space for the data
            uint32_t dataSize = ntohl(requestHeader.dataSize);
            data = std::make_shared<uint8_t>(new uint8_t[dataSize], std::default_delete<uint8_t[]>());
            
            // receive the data
            readBytes = recv(_receiveSocket, data.get(), ntohl(requestHeader.dataSize), 0);
            
            if (readBytes == 0) {
                RGPLOGV("Remote Node closed connection");
                break;
            }
            
            // error check
            if (readBytes < 0) {
                Log::sharedLog()->errorWithErrno("ChordNode::handleRequests():recv2: ", errno);
                continue;
            }
            
            if (readBytes != static_cast<ssize_t>(ntohl(requestHeader.dataSize))) {
                Log::sharedLog()->error((std::string("data size: ") += std::to_string(ntohl(requestHeader.dataSize))
                                         += " readBytes: ") += std::to_string(readBytes));
                Log::sharedLog()->error("recv(): don't received the expected data size");
                break;
            }
        }
        
        // check message type and react appropriate
        switch (requestHeader.type)
        {
                
            case ChordMessageTypeHeartbeat:
            {
                RGPLOGV(std::string("received Heartbeat message from: ") += std::to_string(_nodeID));
                // answer with heartbeat reply
                sendResponse(ChordMessageTypeHeartbeatReply, nullptr, 0);
                
                break;
            }
                
            case ChordMessageTypeSearch:
            {
                RGPLOGV("received Search message");
                
                // error check
                if (!data) {
                    Log::sharedLog()->error("received search without data ...");
                    break;
                }
                
                ChordId key = ntohl(*data);
                
                // search the key (checks local / sends search)
                ChordHeaderNode node = _chord->searchForKey(std::make_shared<ChordNode>(this), key);
                
                // create understandable response format
                std::shared_ptr<uint8_t> nodeData(new uint8_t[sizeof(ChordHeaderNode)], std::default_delete<uint8_t[]>());
                memcpy(nodeData.get(), &node, sizeof(ChordHeaderNode));
                
                // send response
                try {
                    
                    sendResponse(ChordMessageTypeSearchNodeResponse, nodeData, sizeof(ChordHeaderNode));
                    
                } catch (ChordConnectionException &exception) {
                    Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                }
                
                break;
            }
                
            case ChordMessageTypeUpdatePredecessor:
            {
                RGPLOGV(std::string("received Update Predecessor message from: ") += std::to_string(_nodeID));
                
                // Error checking
                if (!data) {
                    Log::sharedLog()->error("received update predecessor without data ...");
                    break;
                }
                
                if (ntohl(requestHeader.dataSize) != sizeof(ChordHeaderNode)) {
                    Log::sharedLog()->error("received update predecessor with unexpected data size ...");
                    break;
                }
                
                // apply new predecessor
                ChordHeaderNode node;
                memcpy(&node, data.get(), sizeof(ChordHeaderNode));
                ChordHeaderNode newPredecessor = _chord->updatePredecessor(node);
                
                // create understandable response format
                std::shared_ptr<uint8_t> nodeData(new uint8_t[sizeof(ChordHeaderNode)], std::default_delete<uint8_t[]>());
                memcpy(nodeData.get(), &newPredecessor, sizeof(ChordHeaderNode));
                
                // send answer
                try {
                    sendResponse(ChordMessageTypePredecessor, nodeData, sizeof(ChordHeaderNode));
                } catch (ChordConnectionException &exception) {
                    Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                }
                
                break;
            }
                
            case ChordMessageTypeDataAdd:
            {
                // someone wants to add data to us
                RGPLOGV("received add data message");
                
                // Error checking
                if (!data) {
                    Log::sharedLog()->error("received add data without data ...");
                    
                    // send answer
                    try {
                        sendResponse(ChordMessageTypeDataAddFailed, nullptr, 0);
                    } catch (ChordConnectionException &exception) {
                        Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                    }
                    
                    break;
                }
                
                bool added = _chord->addDataToHashMap(data);
                
                try {
                    if (added) {
                        // send success answer
                        sendResponse(ChordMessageTypeDataAddSuccess, nullptr, 0);
                    } else {
                        // send failed answer
                        sendResponse(ChordMessageTypeDataAddFailed, nullptr, 0);
                    }
                    
                } catch (ChordConnectionException &exception) {
                    Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                }
                
                break;
            }
                
            case ChordMessageTypeDataRequest:
            {
                RGPLOGV("received data request message");
                
                if (!data) {
                    Log::sharedLog()->error("received data request without data ...");
                    break;
                }
                
                ChordId key = ntohl(*data);
                
                // search for the data
                std::shared_ptr<uint8_t> data = _chord->getDataWithKey(key);
                
                if (data) {
                    
                    uint32_t dataSize { ntohl(*data.get()) };
                    
                    // send response
                    try {
                        sendResponse(ChordMessageTypeDataAnswer, data, dataSize);
                        
                    } catch (ChordConnectionException &exception) {
                        Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                    }
                    
                } else {
                    // send response
                    try {
                        sendResponse(ChordMessageTypeDataNotFound, nullptr, 0);
                        
                    } catch (ChordConnectionException &exception) {
                        Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                    }
                }
                break;
            }
                
            default:
            {
                Log::sharedLog()->error(std::string("received unknown message type: ") += std::to_string(requestHeader.type));
                break;
            }
        }
    }
    
    RGPLOGV("close handlingThread");
    
    close(_receiveSocket);
    _receiveSocket = -1;
}

// sends response to remote node
// throws ChordConnectionException on error
void ChordNode::sendResponse (ChordMessageType type, std::shared_ptr<uint8_t> data, ssize_t dataSize)
{
    // data that will be send later (ChordHeader + the data)
    uint8_t *message = new uint8_t[sizeof(ChordHeader) + dataSize];
    
    // header
    ChordHeader header = _chord->createChordHeader(type);
    header.dataSize = htonl(dataSize);
    
    // copy header to message
    memcpy(message, &header, sizeof(ChordHeader));
    
    if (dataSize > 0) {
        // copy data to message
        memcpy((message + sizeof(ChordHeader)), data.get(), dataSize);
    }
    
    // send message
    ssize_t bytesSend { 0 };
    if ((bytesSend = send(_receiveSocket, message, sizeof(ChordHeader) + dataSize, 0)) <= 0) {
        
        // memory management
        delete [] message; message = nullptr;
        
        // check if connection was closed
        if (bytesSend == 0) {
            Log::sharedLog()->error("Remote Node closed connection");
            throw ChordConnectionException { "Remote Node closed connection" };
        }
        
        // check if send failed
        if (bytesSend == -1) {
            Log::sharedLog()->errorWithErrno("ChordNode::sendResponse():send() ", errno);
            throw ChordConnectionException { "Error sending data to remote node" };
        }
    }
    
    // memory management
    if (message != nullptr) {
        delete [] message; message = nullptr;
    }
}

// sends request to remote node
// throws ChordConnectionException on error
void ChordNode::sendRequest (ChordMessageType type, std::shared_ptr<uint8_t> data, ssize_t dataSize)
{
    // data that will be send later (ChordHeader + the data)
    uint8_t *message = new uint8_t[sizeof(ChordHeader) + dataSize];
    
    // header
    ChordHeader header = _chord->createChordHeader(type);
    header.dataSize = htonl(dataSize);
    
    // copy header to message
    memcpy(message, &header, sizeof(ChordHeader));
    
    if (dataSize > 0) {
        // copy data to message
        memcpy((message + sizeof(ChordHeader)), data.get(), dataSize);
    }
    
    // send message
    ssize_t bytesSend { 0 };
    if ((bytesSend = send(_sendSocket, message, sizeof(ChordHeader) + dataSize, 0)) <= 0) {
        
        delete [] message;
        message = nullptr;
        
        // check if connection was closed
        if (bytesSend == 0) {
            Log::sharedLog()->error("Remote Node closed connection");
            close(_sendSocket);
            _sendSocket = -1;
            throw ChordConnectionException { "Remote Node closed connection" };
        }
        
        // check if send failed
        if (bytesSend == -1) {
            Log::sharedLog()->errorWithErrno("ChordNode::sendRequest():send() ", errno);
            throw ChordConnectionException { "Error sending data to remote node" };
        }
    }
    
    // memory management
    if (message != nullptr) {
        delete [] message;
    }
}

// receives response from remote node
// throws ChordConnectionException on error
std::shared_ptr<uint8_t> ChordNode::recvResponse (ChordMessageType type, ssize_t dataSize)
{
    // receive answer
    ssize_t readBytes { 0 };
    ChordHeader responseHeader;
    
    if ((readBytes = recv(_sendSocket, &responseHeader, sizeof(ChordHeader), 0)) <= 0) {
        
        if (readBytes == 0) { // connection was closed
            Log::sharedLog()->error(std::string("Node with id: ") += std::to_string(_nodeID) += " closed the connection");
            close(_sendSocket);
            _sendSocket = -1;
            throw ChordConnectionException { "Node closed the connection" };
        }
        
        // error
        Log::sharedLog()->errorWithErrno("ChordNode::recvResponse():recv1() ", errno);
        throw ChordConnectionException { strerror(errno) };
    }
    
    // error check
    if (readBytes != sizeof(ChordHeader)) {
        Log::sharedLog()->error("don't received enough data ... something bad happened");
        throw ChordConnectionException { "don't received enough data ... something bad happened" };
    }
    
    type = static_cast<ChordMessageType>(responseHeader.type);
    dataSize = ntohl(responseHeader.dataSize);
    
    if (dataSize > 0) {
        std::shared_ptr<uint8_t> data(new uint8_t[dataSize], std::default_delete<uint8_t[]>());
        
        // receive the data
        if ((readBytes = recv(_sendSocket, data.get(), dataSize, 0)) <= 0) {
            if (readBytes == 0) {
                Log::sharedLog()->error(std::string("Node with id: ") += std::to_string(_nodeID) += " closed the connection");
                close(_sendSocket);
                _sendSocket = -1;
                throw ChordConnectionException { "Node closed the connection" };
            }
            Log::sharedLog()->errorWithErrno("ChordNode::recvResponse():recv2() ", errno);
            throw ChordConnectionException { strerror(errno) };
        }
        
        // error check
        if (readBytes != dataSize) {
            Log::sharedLog()->error("don't received enough data ... something bad happened");
            throw ChordConnectionException { "don't received enough data ... something bad happened" };
        }
        
        return data;
    }
    
    return nullptr;
}
