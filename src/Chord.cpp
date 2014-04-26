/*
 Chord.cpp
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

#include <rgp/Chord.h>
#include <rgp/Log.h>

// standard c
#include <unistd.h>

// standard c++
#include <cmath>
#include <cstring>
#include <sstream>
#include <sys/time.h>

// network
#include <arpa/inet.h>
#include <netdb.h>

using namespace rgp;

#pragma mark - Constructor / Destructor

Chord::Chord(std::string ipAddress, uint16_t port)
{
    // alloc own node
    initOwnNode(ipAddress, port);
    
    // fill finger table with ownNode
    // initialy there is only us in the dht - so we are responsible for all keys
    for (int k=0; k <= kKeyLenght; k++) {
//        int i = static_cast<int>(ownNode->getNodeID() + std::pow(2, k-1)) % static_cast<int>(std::pow(2, kKeyLenght));
        fingerTable.push_back(ownNode);
    }
    
    // we are responsible for all keys
    responsibilityRange = ChordDataRange(0, highestID()); // whole ring
    
    // listen for incomming connections
    connectThread = std::thread(&Chord::waitForIncommingConnections, this);
    
    // start stabilize protocol
    stabilizeThread = std::thread(&Chord::stabilize, this);
}

Chord::Chord(std::string ipAddress, uint16_t port, std::string c_ipAddress, uint16_t c_port)
{
    // alloc own node
    initOwnNode(ipAddress, port);
    
    // listen for incomming connections
    connectThread = std::thread(&Chord::waitForIncommingConnections, this);
    
    // connect to dht overlay
    this->joinDHT(c_ipAddress, c_port);
    
    // start stabilize protocol
    stabilizeThread = std::thread(&Chord::stabilize, this);
    
    // TODO: fill finger table
//    for (int k=1; k < kKeyLenght; k++) {
//        int nodeID = (int)(ownNode->getNodeID() + std::pow(2, k-1)) % highestID();
//        
//    }
}

Chord::~Chord()
{
    // close connectThread
    this->stopConnectThread = true;
    this->stopStabilizeThread = true;
    
    try {
        this->connectThread.join();
    } catch (...) {} // if thread not joinable
    
    try {
        this->stabilizeThread.join();
    } catch (...) {} // if thread not joinable
    
    // dealloc ownNode
    delete this->ownNode;
    
    // TODO: dealloc a lot more (memory management)
}

#pragma mark - Private

void Chord::initOwnNode(std::string ipAddress, uint16_t port)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // choose node id for ourself
    std::srand(static_cast<unsigned int>(tv.tv_usec)); //use current time as seed for random generator
    NodeID_t nodeID { static_cast<NodeID_t>(std::rand() % highestID()) };
    RGPLOGV(std::string("We are Node with ID: ") += std::to_string(nodeID));
    
    this->ownNode = new ChordNode { nodeID, ipAddress, port, this};
}

void Chord::waitForIncommingConnections()
{
    int listening_port = this->ownNode->getPort();
    
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
    while (!this->stopConnectThread) {
        struct sockaddr_in data;
		unsigned int size = sizeof(struct sockaddr_in);
        
        RGPLOGV("Chord::waitForIncommingConnections(): waiting for incomming connection");
        
        // wait for incomming connection
        client_socket = accept(server_socket, (struct sockaddr *)&data, &size);
        
        RGPLOGV("Chord::waitForIncommingConnections(): client connected ...");
        
        if (client_socket > 0) { // < 0 ==> Error
            
            ssize_t readBytes { 0 };
            ChordHeader_t requestHeader { {0, 0, 0}, 0, 0 };
            
            // wait for incomming data
            if ((readBytes = recv(client_socket, &requestHeader, sizeof(ChordHeader_t), 0)) <= 0) {
                if (readBytes == 0) {
                    RGPLOGV("Chord::waitForIncommingConnections(): Remote Node closed connection");
                    continue;
                }
                Log::sharedLog()->errorWithErrno("Chord::waitForIncommingConnections():recv() ", errno);
                continue;
            }
            
            // error check
            if (readBytes != sizeof(ChordHeader_t)) {
                Log::sharedLog()->error("Chord::waitForIncommingConnections(): don't received enough data ... something bad happened");
                continue;
            }
            
            // print received header for debug
            struct in_addr ip { 0 };
            ip.s_addr = ntohl(requestHeader.node.ip);
            
            NodeID_t nodeID { 0 };
            std::string ipAddress { "" };
            uint16_t port { 0 };
            
            switch (requestHeader.type) {
                case ChordMessageTypeIdentify:
                {
                    RGPLOGV("received Identify message");
                    
                    // set values from header
                    nodeID = ntohs(requestHeader.node.nodeID);
                    ipAddress = inet_ntoa(ip);
                    port = ntohs(requestHeader.node.port);
                    
                    // check if there is already a node with this id
                    ChordNode *node = findNodeWithID(nodeID);
                    
                    // don't found node with given id
                    if (node == nullptr) {
                        RGPLOGV("Chord::waitForIncommingConnections(): node don't exists - creating");
                        
                        // create new chord node and append to existing list
                        ChordNode *newChordNode { new ChordNode (nodeID, ipAddress, port, this) };
                        newChordNode->setReceiveSocket(client_socket);
                        
                        connectedNodes_mutex.lock();
                        connectedNodes.push_back(newChordNode);
                        connectedNodes_mutex.unlock();
                    
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
void Chord::joinDHT(std::string c_ipAddress, uint16_t c_port)
{
    // we can use NodeID = 0
    // 1. we don't know the id
    // 2. we will delete this node after using to join the dht
    ChordNode *joinNode = new ChordNode { 0, c_ipAddress, c_port, this };
    ChordConnectionStatus joinStatus = joinNode->establishSendConnection();
    
    // check if connection could be established
    if (joinStatus == ChordConnectionStatusConnectingFailed) {
        Log::sharedLog()->error((std::string("failed to join dht while connecting to: ") += c_ipAddress
                                       += " on port: ") += std::to_string(c_port));
        exit(EXIT_FAILURE); // we cannot join --> terminate app
    }
    
    ChordNode_t successorNode;
    try {
        successorNode = joinNode->searchForKey(ownNode->getNodeID()); // search our id
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(((std::string("failed to join dht while searching for key: ") += std::to_string(ownNode->getNodeID()))
                                       += " with error: ") += exception.what());
        exit(EXIT_FAILURE); // we cannot join --> terminate app
    }
    
    // remove (and disconnect)
    delete joinNode; joinNode = nullptr;
    
    struct in_addr successorIP;
    successorIP.s_addr = ntohl(successorNode.ip);
    this->successor = new ChordNode (ntohs(successorNode.nodeID), inet_ntoa(successorIP), ntohs(successorNode.port), this);
    
    connectedNodes_mutex.lock();
    connectedNodes.push_back(this->successor);
    connectedNodes_mutex.unlock();
    
    RGPLOGV(std::string("received successor node: ") += this->successor->description());
    
    // we shouldn't be responsible for all that, but we may receive keys from our successor,
    // so don't throw them back to successor
    responsibilityRange = ChordDataRange(ntohs(successorNode.nodeID) +1, ownNode->getNodeID());
    
    // connect
    this->successor->establishSendConnection();
    
    // fill finger table 
}

// sets the given node as predecessor
inline void Chord::setPredecessor(ChordNode_t node)
{
    // search for ChordNode and apply to predecessor
    predecessor = findNodeWithID(ntohs(node.nodeID));
    
    // if not found - create node
    if (predecessor == nullptr) {
        
        ChordNode *newPred { nullptr };
        newPred = findNodeWithID(ntohs(node.nodeID)); // check if we have a connection already
        
        if (newPred == nullptr) {
            // we don't have this node yet -> create new
            struct in_addr predecessorIP { ntohl(node.ip) };
            predecessor = new ChordNode(ntohs(node.nodeID), inet_ntoa(predecessorIP), ntohs(node.port), this);
            
            connectedNodes_mutex.lock();
            connectedNodes.push_back(predecessor);
            connectedNodes_mutex.unlock();
        } else {
            predecessor = newPred;
        }
    }
    
    // update responsibility
    responsibilityRange.setFrom(ntohs(node.nodeID) +1);
    responsibilityRange.setTo(ownNode->getNodeID());
    
    // transfer keys
    std::list<ChordData *>dataToTransfer; // list with all data to transfer
    
    // collect all data to transfer
    dataMap_mutex.lock();
    for (auto iterator : dataMap) { // C++11 for each
        
        if (!keyIsInMyRange(iterator.second->getID())) {
            dataToTransfer.push_back(iterator.second);
        }
    }
    
    // remove all the data from local map
    for (auto data : dataToTransfer) {
        dataMap.erase(data->getID());
    }
    dataMap_mutex.unlock();
    
    // send the data to predecessor
    for (auto data : dataToTransfer) {
        std::string dataString { data->getData() };
        
        RGPLOGV("Chord::setPredecessor(): transfer data to predecessor");
        predecessor->establishSendConnection();
        predecessor->addData(&dataString);
        
        //memory management
        delete data;
    }
}

#pragma mark -

void Chord::stabilize()
{
    const int kStandardDelaySeconds { 10 };
    std::chrono::seconds delay_time(1); // first stablize will connect to dht - so do it quick
    
    while (!stopStabilizeThread) {
        
        std::this_thread::sleep_for(delay_time);
        delay_time = std::chrono::seconds (kStandardDelaySeconds); // reset back to standard
        RGPLOGV("stabilize ...");
        
        if (successor == nullptr) {
            if (predecessor != nullptr) {
                
                // stabilize will fix successor now
                // very inefficient, but we don't have a finger table
                successor = predecessor;
                successor->establishSendConnection();
            }
        }
        
        if (successor != nullptr) {
            try {
                ChordNode_t pred = successor->getPredecessorFromRemoteNode(ownNode);
                
                RGPLOGV(((std::string("stabilize (") += std::to_string(ownNode->getNodeID())
                                              += ")... my successors(") += std::to_string(successor->getNodeID()) += ") predecessor: ")
                                              += std::to_string(ntohs(pred.nodeID)));
                
                // check if we are predecessor
                if (ntohs(pred.nodeID) != ownNode->getNodeID()) {
                    
                    // close send connection to successor - we don't need the connection anymore (if node isn't in finger table)
                    successor->closeSendConnection();
                    
                    ChordNode *newSucc { nullptr };
                    newSucc = findNodeWithID(ntohs(pred.nodeID)); // check if we have already a connection to the new successor
                    if (newSucc != nullptr) {
                        RGPLOGV("stabilize newSucc ...");
                        successor = newSucc;
                        successor->establishSendConnection();
                    } else {
                        RGPLOGV("stabilize create new node ...");
                        
                        // create node for successor
                        struct in_addr predIP;
                        predIP.s_addr = ntohl(pred.ip);
                        successor = new ChordNode (ntohs(pred.nodeID), inet_ntoa(predIP), ntohs(pred.port), this);
                        
                        // add successor to list of connected nodes
                        connectedNodes_mutex.lock();
                        connectedNodes.push_back(successor);
                        connectedNodes_mutex.unlock();
                        
                        successor->establishSendConnection();
                        delay_time = std::chrono::seconds (1); // don't wait so long with next poll
                        continue;
                    }
                }
                
            } catch (ChordConnectionException &exception) {
                Log::sharedLog()->error("Chord::stabilize(): error communicating with successor");
                
                // try to connect again
                ChordConnectionStatus succStatus = successor->establishSendConnection();
                
                // successor is dead -> set successor to nullptr
                if (succStatus == ChordConnectionStatusConnectingFailed) {
                    Log::sharedLog()->error("Chord::stabilize(): error can't establish connection to successor " \
                                            "--> setting successor to nullptr");
                    successor = nullptr;
                }
            }
        }
        
        // check that predecessor is alive
        if (predecessor != nullptr) {
            if (!predecessor->isAlive()) {
                
                RGPLOGV("Chord::stabilize(): my predecessor died...");
                
                // predecessor died -> remove from connected list
                connectedNodes_mutex.lock();
                connectedNodes.remove(predecessor);
                connectedNodes_mutex.unlock();
                
                // no predecessor -> set predecessor to nullptr
                predecessor = nullptr;
            }
        }
        
        /// memory management
        // cleanup connectedThreads list
        connectedNodes_mutex.lock();
        std::list<ChordNode *>nodesToDelete;
        for (ChordNode *node : connectedNodes) { // C++11 for each
            if (node != successor && node != predecessor) { // don't delete successor or predecessor
                if (!node->isAlive()) {
                    // if dead remove node
                    nodesToDelete.push_back(node);
                    // we cannot remove it directly -> altering the list while iterating -> segv
                }
            }
        }
        
        // delete all nodes now
        for(ChordNode *node : nodesToDelete) {
            connectedNodes.remove(node);
            delete node;
        }
        connectedNodes_mutex.unlock();
    }
}

// TODO: implement fix_fingers
void Chord::fix_fingers()
{
    /*
     Jeder Knoten Kn führt periodisch eine fix_fingers() Funktion aus:
     -  Diese iteriert über die Einträge der Fingertable
     (oder per Zufall einen auswählen)
     -  Und sucht für jeden Eintrag i den aktuell
     gültigen Nachfolger von n + 2i-1
     */
}

#pragma mark - Public: Called by Chord, ChordNode and Main

// helper for daemon process - main thread will block here
void Chord::join()
{
    try {
        connectThread.join();
    } catch (...) {} // if thread can't be joined
}

// creates a chord header with the well known data and the given type
// if there needs to be data appended - this need to be added later
// the returned value needs to be deleted
ChordHeader_t *Chord::createChordHeader(ChordMessageType type)
{
    // create header
    ChordHeader_t *header = new ChordHeader_t;
    
    // fill header with well known data
    header->node.nodeID = htons(ownNode->getNodeID());
    header->node.ip = htonl(inet_addr(ownNode->getIPAddress().c_str()));
    header->node.port = htons(ownNode->getPort());
    header->dataSize = 0;
    
    // set given type
    header->type = type;
    
    // return the filled header
    return header;
}

// highest possible hash id
int Chord::highestID() const
{
    return static_cast<int>(std::pow(2, kKeyLenght) -1);
}

// check if i'm responsible for the given key
bool Chord::keyIsInMyRange(DataID_t key) const
{
    // special case
    if (responsibilityRange.getFrom() >= responsibilityRange.getTo()) {
        
        if (key >= responsibilityRange.getFrom() || key <= responsibilityRange.getTo()) {
            return true;
        }
        
    } else {
        
        // check if key is inside my responsible range
        if (key >= responsibilityRange.getFrom() && key <= responsibilityRange.getTo()) {
            return true;
        }
    }

    return false;
}

ChordNode_t Chord::searchForKey(ChordNode *searchingNode, DataID_t key) const
{
    RGPLOGV(std::string("search for key: ") += std::to_string(key));
    ChordNode_t responsibleNode { 0, 0, 0};
    
    // return ownNode if i'm responsible
    if (keyIsInMyRange(key)) {
        
        responsibleNode.nodeID = htons(ownNode->getNodeID());
        responsibleNode.ip = htonl(inet_addr(ownNode->getIPAddress().c_str()));
        responsibleNode.port = htons(ownNode->getPort());
        
        RGPLOGV(std::string("return responsible node: ") += std::to_string(ownNode->getNodeID()));
        return responsibleNode;
    }
    
    // if i'm not responsible - search using successor
    // TODO: use fingertable
    try {
        bool searchDone = false;
        
        // don't send search back to where it came from
        if (searchingNode != predecessor && predecessor != nullptr) {
            
            // check if searching node may don't know the existence of my predecessor (and possibly skipped the node)
            if (searchingNode->getNodeID() < key && predecessor->getNodeID() > key) {
                
                // search with predecessor
                RGPLOGV(std::string("i'm not responsible - passthrough search (predecessor): ") += std::to_string(predecessor->getNodeID()));
                predecessor->establishSendConnection();
                responsibleNode = predecessor->searchForKey(key);
                
                searchDone = true;
            }
        }
        
        // only if not searched with predecessor already
        if (!searchDone) {
            
            // don't send search back to where it came from
            if (searchingNode != successor && successor != nullptr) {
                
                RGPLOGV(std::string("i'm not responsible - passthrough search (successor): ") += std::to_string(successor->getNodeID()));
                responsibleNode = successor->searchForKey(key);
            } else {
                
                // if we can't find a responsible node -> what to do now ?
                // i return myself -> helps joining nodes, but won't help if search was for adding / receiving data
                responsibleNode.nodeID = htons(ownNode->getNodeID());
                responsibleNode.ip = htonl(inet_addr(ownNode->getIPAddress().c_str()));
                responsibleNode.port = htons(ownNode->getPort());
            }
        }
        
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("Chord::searchForKey: ") += exception.what());
        // TODO: what to do if we can't receive the responsible node ?
        // if we can't find a responsible node return self -> what to do now ?
        responsibleNode.nodeID = htons(ownNode->getNodeID());
        responsibleNode.ip = htonl(inet_addr(ownNode->getIPAddress().c_str()));
        responsibleNode.port = htons(ownNode->getPort());
    }
    
    RGPLOGV((std::string("search result for key (") += std::to_string(key) += ") ") += std::to_string(ntohs(responsibleNode.nodeID)));
    
    return responsibleNode;
}

// update predecessor if needed
// returns new predecessor
ChordNode_t Chord::updatePredecessor(ChordNode_t node)
{
    // if there is currently no predecessor
    // just accept
    if (predecessor == nullptr) {
        
        setPredecessor(node);
    } else
    
    // check special case
    if (predecessor->getNodeID() > ownNode->getNodeID()) {
        // new predecessor between 0 and my node id
        if (ntohs(node.nodeID) < ownNode->getNodeID()) {
            
            setPredecessor(node);
        } else
            
            // new predecessor between my old predecessor and me
            if (ntohs(node.nodeID) > predecessor->getNodeID()) {
                setPredecessor(node);
            }
    } else
    
    // accept if new predecessor fits
    if (ntohs(node.nodeID) > predecessor->getNodeID() && ntohs(node.nodeID) < ownNode->getNodeID()) {
        
        setPredecessor(node);
    }
    
    return predecessor->chordNode_t();
}

// searches all data from chord for given node id
// returns nullptr if not found
ChordNode *Chord::findNodeWithID(NodeID_t nodeID)
{
    // check if node is ownNode
    if (ownNode->getNodeID() == nodeID) {
        return ownNode;
    }
    
    // check if node is successor
    if (successor != nullptr) {
        if (successor->getNodeID() == nodeID) {
            return successor;
        }
    }
    
    // check if node is predecessor
    if (predecessor != nullptr) {
        if (predecessor->getNodeID() == nodeID) {
            return predecessor;
        }
    }
    
    // TODO: check if node is in fingertable
    
    // check if node is in connected node list
    connectedNodes_mutex.lock();
    for (ChordNode *node : connectedNodes) {
        if (node->getNodeID() == nodeID) {
            
            connectedNodes_mutex.unlock();
            return node;
        }
    }
    connectedNodes_mutex.unlock();
    
    return nullptr;
}

// adds data from remote node to local data map - if we are responsible
// returns true on success
// returns false if we aren't responsible
bool Chord::addDataToHashMap(std::string data)
{
    // create hash
    int dataHash = std::hash<std::string>()(data) % highestID();
    
    RGPLOGV((std::string("Chord::addDataToHashMap(): ") += std::to_string(dataHash) += " data: ") += data);
    
//    if (keyIsInMyRange(dataHash)) {
        // add data to dataMap
        dataMap_mutex.lock();
        dataMap[dataHash] = new ChordData (dataHash, data); // hint: if there was already a value it will be replaced --> memory leak
        dataMap_mutex.unlock();
        
        return true;
//    }
    
//    return false;
}

// searches local data for data id and returns the data
// returns nullptr if there is no data with that key
ChordData *Chord::getDataWithKey(DataID_t dataID)
{
    dataMap_mutex.lock();
    
    // find id in hash map
    auto iterator = dataMap.find(dataID);
    
    // check if in hash map
    if (iterator != dataMap.end()) {
        
        dataMap_mutex.unlock();
        
        // returns the data
        return iterator->second;
    }
    
    dataMap_mutex.unlock();
    
    // not found
    return nullptr;
}

#pragma mark - Public: Called by User

// PUT command
void Chord::addData(std::string data)
{
    // create hash
    int dataHash = std::hash<std::string>()(data) % highestID();
    
    // we are responsible for the data
    if (keyIsInMyRange(dataHash)) {
        
        // add data to dataMap
        dataMap_mutex.lock();
        dataMap[dataHash] = new ChordData (dataHash, data); // hint: if there was already a value it will be replaced --> memory leak
        dataMap_mutex.unlock();
        
        RGPLOG(std::string("hash: ") += std::to_string(dataHash));
        RGPLOG(std::string("node: ") += ownNode->description());
        
    } else {
        
        // we are not responsible -> search for the responsible node
        ChordNode_t responsibleNode = searchForKey(ownNode, dataHash);
        
        if (responsibleNode.port == 0) { // port == 0 indicates that we don't found a responsible node
            
            RGPLOG("couldn't add data: don't found responsible node");
            
        } else {
            
            // send data to node
            ChordNode *node = findNodeWithID(ntohs(responsibleNode.nodeID));
            
            if (node == nullptr) { // we don't have the node yet
                
                connectedNodes_mutex.lock();
                
                // create new node
                struct in_addr ip { 0 };
                ip.s_addr = ntohl(responsibleNode.ip);
                
                node = new ChordNode (ntohs(responsibleNode.nodeID), inet_ntoa(ip), ntohs(responsibleNode.port), this);
                
                // add node to list
                connectedNodes.push_back(node);
                
                connectedNodes_mutex.unlock();
            }
            
            node->establishSendConnection(); // create send connection if needed
            
            bool success { node->addData(&data) }; // add the data to the remote node
            
            if (success) {
                
                RGPLOG(std::string("hash: ") += std::to_string(dataHash));
                RGPLOG(std::string("node: ") += node->description());
                
            } else {
                RGPLOG((std::string("failed to add data to node: ") += std::to_string(node->getNodeID()) += " with data id: ") += std::to_string(dataHash));
            }
            
            if (node != successor && node != predecessor) { // if we have a finger table -> don't disconnect from finger table nodes too
                node->closeSendConnection();
            }
        }
    }
}

// GET command
void Chord::printDataWithHash(DataID_t hash)
{
    ChordNode_t node = searchForKey(ownNode, hash);
    
    // error check
    if (node.port == 0) {
        RGPLOG(std::string("couldn't find responsible node for key: ") += std::to_string(hash));
        return;
    }
    
    ChordNode *responsibleNode = findNodeWithID(ntohs(node.nodeID));
    
    if (responsibleNode == nullptr) { // we don't have the node yet
        
        connectedNodes_mutex.lock();
        
        // create new node
        struct in_addr ip { 0 };
        ip.s_addr = ntohl(node.ip);
        
        responsibleNode = new ChordNode (ntohs(node.nodeID), inet_ntoa(ip), ntohs(node.port), this);
        
        // add node to list
        connectedNodes.push_back(responsibleNode);
        
        connectedNodes_mutex.unlock();
    }
    
    // create send connection if needed
    responsibleNode->establishSendConnection();
    
    // receive the data from the responsible node
    ChordData *data = responsibleNode->requestDataForKey(hash);
    
    if (responsibleNode != successor && responsibleNode != predecessor) { // if we have a finger table -> don't disconnect from finger table nodes too
        responsibleNode->closeSendConnection();
    }
    
    // error check
    if (data == nullptr) {
        RGPLOG(std::string("couldn't find data for key: ") += std::to_string(hash));
        return;
    }
    
    // print the data
    RGPLOG(std::string("value: ") += data->getData());
    RGPLOG(std::string("node: ") += responsibleNode->description());
    
    // memory management
    delete data;
}

// LIST command
void Chord::printAllLocalData()
{
    dataMap_mutex.lock();
    // check if data is empty
    if (dataMap.size() <= 0) {
        Log::sharedLog()->print("no local data");
        
    } else {
        
        // iterate through all data in map
        for (auto iterator : dataMap) { // C++11 for each loop
            
            ChordData value = *iterator.second;
            Log::sharedLog()->print((std::string() += std::to_string(value.getID()) += " --> ") += value.getData());
        }
    }
    dataMap_mutex.unlock();
}

// prints some status data (successor, predecessor, ...)
void Chord::printStatus()
{
    // OwnNode
    Log::sharedLog()->print(std::string("I'm Node: ") +=  ownNode->description());
    
    // Successor
    if (successor != nullptr) {
        Log::sharedLog()->print(std::string("Successor: ") += successor->description());
    } else {
        Log::sharedLog()->print("Currently no Successor... ");
    }
    
    // Predecessor
    if (predecessor != nullptr) {
        Log::sharedLog()->print(std::string("Predecessor: ") += predecessor->description());
    } else {
        Log::sharedLog()->print("Currently no Predecessor... ");
    }
    
    // Responsible range
    Log::sharedLog()->print((std::string("My key range: ") += std::to_string(responsibilityRange.getFrom())
                                   += " - ") += std::to_string(responsibilityRange.getTo()));
    
    connectedNodes_mutex.lock();
    Log::sharedLog()->print("connected nodes:");
    for (ChordNode *node : connectedNodes) {
        Log::sharedLog()->print(node->description());
    }
    connectedNodes_mutex.unlock();
}
