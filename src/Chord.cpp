/*
 Chord.cpp
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

#include <rgp/Chord.h>
#include <rgp/Log.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <complex>
#include <sys/time.h>

using namespace rgp;

#pragma mark - Constructor / Destructor

Chord::Chord (std::string ipAddress, uint16_t port)
{
    // alloc own node
    initOwnNode(ipAddress, port);
    
    // fill finger table with ownNode
    // initialy there is only us in the dht - so we are responsible for all keys
    for (int k=0; k <= kKeyLenght; k++) {
//        int i = static_cast<int>(ownNode->getNodeID() + std::pow(2, k-1)) % static_cast<int>(std::pow(2, kKeyLenght));
        _fingerTable.push_back(_ownNode);
    }
    
    // we are responsible for all keys
    _responsibilityRange = (ChordRange) { .from=0, .to=highestID() }; // whole ring
    
    // listen for incomming connections
    _connectThread = std::thread(&Chord::waitForIncommingConnections, this);
    
    // start stabilize protocol
    _stabilizeThread = std::thread(&Chord::stabilize, this);
}

Chord::Chord (std::string ipAddress, uint16_t port, std::string c_ipAddress, uint16_t c_port)
{
    // alloc own node
    initOwnNode(ipAddress, port);
    
    // listen for incomming connections
    _connectThread = std::thread(&Chord::waitForIncommingConnections, this);
    
    // connect to dht overlay
    joinDHT(c_ipAddress, c_port);
    
    // start stabilize protocol
    _stabilizeThread = std::thread(&Chord::stabilize, this);
    
// TODO: fill finger table
//    for (int k=1; k < kKeyLenght; k++) {
//        int nodeID = (int)(ownNode->getNodeID() + std::pow(2, k-1)) % highestID();
//
//    }
}

Chord::~Chord ()
{
    // close connectThread
    _stopConnectThread = true;
    _stopStabilizeThread = true;
    
    try {
        _connectThread.join();
    } catch (...) {} // if thread not joinable
    
    try {
        _stabilizeThread.join();
    } catch (...) {} // if thread not joinable
}

#pragma mark - Public

// creates a chord header with the well known data and the given type
// if there needs to be data appended - this need to be added later
// the returned value needs to be deleted
ChordHeader Chord::createChordHeader (ChordMessageType type)
{
    // create header
    ChordHeader header { };
    
    // fill header with well known data
    header.node.nodeId = htons(_ownNode->getNodeID());
    header.node.ip = htonl(inet_addr(_ownNode->getIPAddress().c_str()));
    header.node.port = htons(_ownNode->getPort());
    header.dataSize = 0;
    
    // set given type
    header.type = type;
    
    // return the filled header
    return header;
}

// highest possible hash id
ChordId Chord::highestID () const
{
    return static_cast<ChordId>(std::pow(2, kKeyLenght) -1);
}

// check if i'm responsible for the given key
bool Chord::keyIsInMyRange (ChordId key) const
{
    // special case
    if (_responsibilityRange.from >= _responsibilityRange.to) {
        
        if (key >= _responsibilityRange.from || key <= _responsibilityRange.to) {
            return true;
        }
        
    } else {
        
        // check if key is inside my responsible range
        if (key >= _responsibilityRange.from && key <= _responsibilityRange.to) {
            return true;
        }
    }
    
    return false;
}

ChordHeaderNode Chord::searchForKey (ChordId searchingNode, ChordId key) const
{
    RGPLOGV(std::string("search for key: ") += std::to_string(key));
    ChordHeaderNode responsibleNode { 0 };
    
    // return ownNode if i'm responsible
    if (keyIsInMyRange(key)) {
        
        responsibleNode.nodeId = htons(_ownNode->getNodeID());
        responsibleNode.ip = htonl(inet_addr(_ownNode->getIPAddress().c_str()));
        responsibleNode.port = htons(_ownNode->getPort());
        
        RGPLOGV(std::string("return responsible node: ") += std::to_string(_ownNode->getNodeID()));
        return responsibleNode;
    }
    
    // if i'm not responsible - search using successor
    // TODO: use fingertable
    try {
        bool searchDone = false;
        
        // don't send search back to where it came from
        if (_predecessor) {
            
            if (_predecessor->getNodeID() != searchingNode) {
                // check if searching node may don't know the existence of my predecessor (and possibly skipped the node)
                if (searchingNode < key && _predecessor->getNodeID() > key) {
                    
                    // search with predecessor
                    RGPLOGV(std::string("i'm not responsible - passthrough search (predecessor): ") += std::to_string(_predecessor->getNodeID()));
                    _predecessor->establishSendConnection();
                    responsibleNode = _predecessor->searchForKey(key);
                    
                    searchDone = true;
                }
            }
        }
        
        // only if not searched with predecessor already
        if (!searchDone) {
            
            // don't send search back to where it came from
            if (_successor) {
                if (_successor->getNodeID() != searchingNode) {
                    RGPLOGV(std::string("i'm not responsible - passthrough search (successor): ") += std::to_string(_successor->getNodeID()));
                    responsibleNode = _successor->searchForKey(key);
                } else {
                    
                    // if we can't find a responsible node -> what to do now ?
                    // i return myself -> helps joining nodes, but won't help if search was for adding / receiving data
                    responsibleNode.nodeId = htons(_ownNode->getNodeID());
                    responsibleNode.ip = htonl(inet_addr(_ownNode->getIPAddress().c_str()));
                    responsibleNode.port = htons(_ownNode->getPort());
                }
            } else {
                
                // if we can't find a responsible node -> what to do now ?
                // i return myself -> helps joining nodes, but won't help if search was for adding / receiving data
                responsibleNode.nodeId = htons(_ownNode->getNodeID());
                responsibleNode.ip = htonl(inet_addr(_ownNode->getIPAddress().c_str()));
                responsibleNode.port = htons(_ownNode->getPort());
            }
        }
        
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("Chord::searchForKey: ") += exception.what());
        // TODO: what to do if we can't receive the responsible node ?
        // if we can't find a responsible node return self -> what to do now ?
        responsibleNode.nodeId = htons(_ownNode->getNodeID());
        responsibleNode.ip = htonl(inet_addr(_ownNode->getIPAddress().c_str()));
        responsibleNode.port = htons(_ownNode->getPort());
    }
    
    RGPLOGV((std::string("search result for key (") += std::to_string(key) += ") ") += std::to_string(ntohs(responsibleNode.nodeId)));
    
    return responsibleNode;
}

// update predecessor if needed
// returns new predecessor
ChordHeaderNode Chord::updatePredecessor (ChordHeaderNode node)
{
    // if there is currently no predecessor
    // just accept
    if (!_predecessor) {
        
        setPredecessor(node);
    } else
        
        // check special case
        if (_predecessor->getNodeID() > _ownNode->getNodeID()) {
            // new predecessor between 0 and my node id
            if (ntohs(node.nodeId) < _ownNode->getNodeID()) {
                
                setPredecessor(node);
            } else
                
                // new predecessor between my old predecessor and me
                if (ntohs(node.nodeId) > _predecessor->getNodeID()) {
                    setPredecessor(node);
                }
        } else
            
            // accept if new predecessor fits
            if (ntohs(node.nodeId) > _predecessor->getNodeID() && ntohs(node.nodeId) < _ownNode->getNodeID()) {
                
                setPredecessor(node);
            }
    
    return _predecessor->chordNode();
}

// searches all data from chord for given node id
// returns nullptr if not found
std::shared_ptr<ChordNode> Chord::findNodeWithId (ChordId nodeId)
{
    // check if node is ownNode
    if (_ownNode->getNodeID() == nodeId) {
        return _ownNode;
    }
    
    // check if node is successor
    if (_successor) {
        if (_successor->getNodeID() == nodeId) {
            return _successor;
        }
    }
    
    // check if node is predecessor
    if (_predecessor) {
        if (_predecessor->getNodeID() == nodeId) {
            return _predecessor;
        }
    }
    
    // TODO: check if node is in fingertable
    
    // check if node is in connected node list
    _connectedNodes_mutex.lock();
    for (std::shared_ptr<ChordNode> node : _connectedNodes) {
        if (node->getNodeID() == nodeId) {
            
            _connectedNodes_mutex.unlock();
            return node;
        }
    }
    _connectedNodes_mutex.unlock();
    
    return nullptr;
}

// adds data from remote node to local data map - if we are responsible
// returns true on success
// returns false if we aren't responsible
bool Chord::addDataToHashMap (std::shared_ptr<uint8_t> data)
{
    // create hash
    int dataHash = std::hash<std::shared_ptr<uint8_t>>()(data) % highestID();
    
    RGPLOGV((std::string("Chord::addDataToHashMap(): ") += std::to_string(dataHash)));
    
    if (keyIsInMyRange(dataHash)) {
        // add data to dataMap
        _dataMap_mutex.lock();
        _dataMap[dataHash] = data; // hint: if there was already a value it will be replaced
        _dataMap_mutex.unlock();
        
        return true;
    }
    
    return false;
}

// searches local data for data id and returns the data
// returns nullptr if there is no data with that key
std::shared_ptr<uint8_t> Chord::getDataWithKey (ChordId dataId)
{
    _dataMap_mutex.lock();
    
    // find id in hash map
    auto iterator = _dataMap.find(dataId);
    
    // check if in hash map
    if (iterator != _dataMap.end()) {
        
        _dataMap_mutex.unlock();
        
        // returns the data
        return iterator->second;
    }
    
    _dataMap_mutex.unlock();
    
    // not found
    return nullptr;
}


#pragma mark - Private

void Chord::initOwnNode (std::string ipAddress, uint16_t port)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // choose node id for ourself
    std::srand(static_cast<unsigned int>(tv.tv_usec)); //use current time as seed for random generator
    ChordId nodeId { static_cast<ChordId>(std::rand() % highestID()) };
    RGPLOGV(std::string("We are Node with ID: ") += std::to_string(nodeId));
    _ownNode = std::make_shared<ChordNode>(nodeId, ipAddress, port, shared_from_this());
}

void Chord::waitForIncommingConnections ()
{
    int listening_port = _ownNode->getPort();
    
    // sockets
    struct sockaddr_in server_sockaddr;
	int client_socket; // data socket
	int opt { 1 }; // socket options
    
    // init struct
    memset(&server_sockaddr, 0, sizeof(server_sockaddr)); // fill struct with zeros
	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_addr.s_addr = INADDR_ANY;
	server_sockaddr.sin_port = htons(listening_port);
    
    // init server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    bind(server_socket, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr));
    listen(server_socket, 20);
    
    // handle incomming connects
    while (!_stopConnectThread) {
        struct sockaddr_in data;
		unsigned int size = sizeof(struct sockaddr_in);
        
        RGPLOGV("Chord::waitForIncommingConnections(): waiting for incomming connection");
        
        // wait for incomming connection
        client_socket = accept(server_socket, (struct sockaddr *)&data, &size);
        
        RGPLOGV("Chord::waitForIncommingConnections(): client connected ...");
        
        if (client_socket > 0) { // < 0 ==> Error
            
            ssize_t readBytes { 0 };
            ChordHeader requestHeader { };
            
            // wait for incomming data
            if ((readBytes = recv(client_socket, &requestHeader, sizeof(ChordHeader), 0)) <= 0) {
                if (readBytes == 0) {
                    RGPLOGV("Chord::waitForIncommingConnections(): Remote Node closed connection");
                    continue;
                }
                Log::sharedLog()->errorWithErrno("Chord::waitForIncommingConnections():recv() ", errno);
                continue;
            }
            
            // error check
            if (readBytes != sizeof(ChordHeader)) {
                Log::sharedLog()->error("Chord::waitForIncommingConnections(): don't received enough data ... something bad happened");
                continue;
            }
            
            // print received header for debug
            struct in_addr ip { 0 };
            ip.s_addr = ntohl(requestHeader.node.ip);
            
            ChordId nodeId { 0 };
            std::string ipAddress { "" };
            uint16_t port { 0 };
            
            switch (requestHeader.type) {
                case ChordMessageTypeIdentify:
                {
                    RGPLOGV("received Identify message");
                    
                    // set values from header
                    nodeId = ntohs(requestHeader.node.nodeId);
                    ipAddress = inet_ntoa(ip);
                    port = ntohs(requestHeader.node.port);
                    
                    // check if there is already a node with this id
                    std::shared_ptr<ChordNode> node = findNodeWithId(nodeId);
                    
                    // don't found node with given id
                    if (node == nullptr) {
                        RGPLOGV("Chord::waitForIncommingConnections(): node don't exists - creating");
                        
                        // create new chord node and append to existing list
                        std::shared_ptr<ChordNode> newChordNode;
                        newChordNode = std::make_shared<ChordNode>(nodeId, ipAddress, port, shared_from_this());
                        newChordNode->setReceiveSocket(client_socket);
                        
                        _connectedNodes_mutex.lock();
                        _connectedNodes.push_back(newChordNode);
                        _connectedNodes_mutex.unlock();
                        
                    } else {
                        RGPLOGV("Chord::waitForIncommingConnections(): already exists - setting receive socket");
                        // start receiving messages
                        node->setReceiveSocket(client_socket);
                    }
                    
                    break;
                }
                    
                default:
                {
                    RGPLOGV("Chord::waitForIncommingConnections(): node don't identified - close connection");
                    close(client_socket);
                    client_socket = 0;
                    break;
                }
            }
            
        } else {
            Log::sharedLog()->errorWithErrno("Chord::waitForIncommingConnections():recv():error using client socket: ", errno);
        }
    }
}

// join existing DHT using given ip and port
void Chord::joinDHT (std::string c_ipAddress, uint16_t c_port)
{
    // we can use NodeID = 0
    // 1. we don't know the id
    // 2. we will delete this node after using to join the dht
    
    std::shared_ptr<ChordNode> joinNode;
    joinNode = std::make_shared<ChordNode>(0, c_ipAddress, c_port, shared_from_this());
    ChordConnectionStatus joinStatus = joinNode->establishSendConnection();
    
    // check if connection could be established
    if (joinStatus == ChordConnectionStatusConnectingFailed) {
        Log::sharedLog()->error((std::string("failed to join dht while connecting to: ") += c_ipAddress
                                 += " on port: ") += std::to_string(c_port));
        exit(EXIT_FAILURE); // we cannot join --> terminate app
    }
    
    ChordHeaderNode successorNode;
    try {
        successorNode = joinNode->searchForKey(_ownNode->getNodeID()); // search our id
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(((std::string("failed to join dht while searching for key: ") += std::to_string(_ownNode->getNodeID()))
                                 += " with error: ") += exception.what());
        exit(EXIT_FAILURE); // we cannot join --> terminate app
    }
    struct in_addr successorIP;
    successorIP.s_addr = ntohl(successorNode.ip);
    _successor = std::make_shared<ChordNode>(ntohl(successorNode.nodeId), inet_ntoa(successorIP), ntohs(successorNode.port), shared_from_this());
    
    _connectedNodes_mutex.lock();
    _connectedNodes.push_back(_successor);
    _connectedNodes_mutex.unlock();
    
    RGPLOGV(std::string("received successor node: ") += _successor->description());
    
    // we shouldn't be responsible for all that, but we may receive keys from our successor,
    // so don't throw them back to successor
    _responsibilityRange = (ChordRange){ .from=static_cast<ChordId>(ntohs(successorNode.nodeId) +1), .to=_ownNode->getNodeID()};
    
    // connect
    _successor->establishSendConnection();
    
    // fill finger table
}

// sets the given node as predecessor
inline void Chord::setPredecessor (ChordHeaderNode node)
{
    // search for ChordNode and apply to predecessor
    _predecessor = findNodeWithId(ntohs(node.nodeId));
    
    // if not found - create node
    if (!_predecessor) {
        
        std::shared_ptr<ChordNode> newPred;
        newPred = findNodeWithId(ntohs(node.nodeId)); // check if we have a connection already
        
        if (newPred == nullptr) {
            // we don't have this node yet -> create new
            struct in_addr predecessorIP { ntohl(node.ip) };
            _predecessor = std::make_shared<ChordNode>(ntohl(node.nodeId), inet_ntoa(predecessorIP), ntohs(node.port), shared_from_this());
            
            _connectedNodes_mutex.lock();
            _connectedNodes.push_back(_predecessor);
            _connectedNodes_mutex.unlock();
        } else {
            _predecessor = newPred;
        }
    }
    
    // update responsibility
    _responsibilityRange.from = ntohs(node.nodeId) +1;
    _responsibilityRange.to = _ownNode->getNodeID();
    
    // transfer keys
    std::list<std::shared_ptr<uint8_t>> dataToTransfer; // list with all data to transfer
    
    // collect all data to transfer
    _dataMap_mutex.lock();
    for (auto iterator : _dataMap) {
        
        int dataHash = std::hash<std::shared_ptr<uint8_t>>()(iterator.second) % highestID();
        
        if (!keyIsInMyRange(dataHash)) {
            dataToTransfer.push_back(iterator.second);
        }
    }
    
    // remove all the data from local map
    for (auto data : dataToTransfer) {
        int dataHash = std::hash<std::shared_ptr<uint8_t>>()(data) % highestID();
        _dataMap.erase(dataHash);
    }
    _dataMap_mutex.unlock();
    
    // send the data to predecessor
    for (auto data : dataToTransfer) {
        
        RGPLOGV("Chord::setPredecessor(): transfer data to predecessor");
        _predecessor->establishSendConnection();
        _predecessor->addData(data);
    }
}

#pragma mark -

void Chord::stabilize ()
{
    const int kStandardDelaySeconds { 10 };
    std::chrono::seconds delay_time(1); // first stablize will connect to dht - so do it quick
    
    while (!_stopStabilizeThread) {
        
        std::this_thread::sleep_for(delay_time);
        delay_time = std::chrono::seconds (kStandardDelaySeconds); // reset back to standard
        RGPLOGV("stabilize ...");
        
        if (!_successor) {
            if (_predecessor) {
                
                // stabilize will fix successor now
                // very inefficient, but we don't have a finger table
                _successor = _predecessor;
                _successor->establishSendConnection();
            }
        }
        
        if (_successor) {
            try {
                ChordHeaderNode pred = _successor->getPredecessorFromRemoteNode(_ownNode);
                
                RGPLOGV(((std::string("stabilize (") += std::to_string(_ownNode->getNodeID())
                          += ")... my successors(") += std::to_string(_successor->getNodeID()) += ") predecessor: ")
                        += std::to_string(ntohs(pred.nodeId)));
                
                // check if we are predecessor
                if (ntohs(pred.nodeId) != _ownNode->getNodeID()) {
                    
                    // close send connection to successor - we don't need the connection anymore (if node isn't in finger table)
                    _successor->closeSendConnection();
                    
                    // check if we have already a connection to the new successor
                    std::shared_ptr<ChordNode> newSucc { findNodeWithId(ntohs(pred.nodeId)) };
                    
                    if (newSucc) {
                        RGPLOGV("stabilize newSucc ...");
                        _successor = newSucc;
                        _successor->establishSendConnection();
                    } else {
                        RGPLOGV("stabilize create new node ...");
                        
                        // create node for successor
                        struct in_addr predIP;
                        predIP.s_addr = ntohl(pred.ip);
                        _successor = std::make_shared<ChordNode>(ntohs(pred.nodeId), inet_ntoa(predIP), ntohs(pred.port), shared_from_this());
                        
                        // add successor to list of connected nodes
                        _connectedNodes_mutex.lock();
                        _connectedNodes.push_back(_successor);
                        _connectedNodes_mutex.unlock();
                        
                        _successor->establishSendConnection();
                        delay_time = std::chrono::seconds (1); // don't wait so long with next poll
                        continue;
                    }
                }
                
            } catch (ChordConnectionException &exception) {
                Log::sharedLog()->error("Chord::stabilize(): error communicating with successor");
                
                // try to connect again
                ChordConnectionStatus succStatus = _successor->establishSendConnection();
                
                // successor is dead -> set successor to nullptr
                if (succStatus == ChordConnectionStatusConnectingFailed) {
                    Log::sharedLog()->error("Chord::stabilize(): error can't establish connection to successor " \
                                            "--> setting successor to nullptr");
                    _successor.reset();
                }
            }
        }
        
        // check that predecessor is alive
        if (_predecessor) {
            if (!_predecessor->isAlive()) {
                
                RGPLOGV("Chord::stabilize(): my predecessor died...");
                
                // predecessor died -> remove from connected list
                _connectedNodes_mutex.lock();
                _connectedNodes.remove(_predecessor);
                _connectedNodes_mutex.unlock();
                
                // no predecessor -> set predecessor to nullptr
                _predecessor.reset();
            }
        }
        
        /// memory management
        // cleanup connectedThreads list
        _connectedNodes_mutex.lock();
        std::list<std::shared_ptr<ChordNode>> nodesToDelete;
        for (std::shared_ptr<ChordNode> node : _connectedNodes) {
            if (node != _successor && node != _predecessor) { // don't delete successor or predecessor
                if (!node->isAlive()) {
                    // if dead remove node
                    nodesToDelete.push_back(node);
                    // we cannot remove it directly -> altering the list while iterating -> segv
                }
            }
        }
        
        // delete all nodes now
        for(std::shared_ptr<ChordNode> node : nodesToDelete) {
            _connectedNodes.remove(node);
        }
        _connectedNodes_mutex.unlock();
    }
}

// TODO: implement fix_fingers
void Chord::fixFingers ()
{
    /*
     Jeder Knoten Kn führt periodisch eine fix_fingers() Funktion aus:
     -  Diese iteriert über die Einträge der Fingertable
     (oder per Zufall einen auswählen)
     -  Und sucht für jeden Eintrag i den aktuell
     gültigen Nachfolger von n + 2i-1
     */
}
