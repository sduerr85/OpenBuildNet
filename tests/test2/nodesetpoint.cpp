/* -*- mode: C++; indent-tabs-mode: nil; -*- */
/** \file
 * \brief Node that implements a simple setpoint.
 *
 * Requires YARP.
 *
 * This file is part of the openBuildNet simulation framework
 * (OBN-Sim) developed at EPFL.
 *
 * \author Truong X. Nghiem (xuan.nghiem@epfl.ch)
 */


#include <fstream>
#include <chrono>
#include <random>
#include <obnnode.h>

#ifndef OBNSIM_COMM_YARP
#error This test requires YARP to run
#endif


/** The following macros are defined by CMake to indicate which libraries this SMN build supports:
 - OBNSIM_COMM_YARP: if YARP is supported for communication.
 - OBNSIM_SMN_COMM_MQTT: if MQTT is supported for communication.
 */

using namespace OBNnode;

#define MAIN_UPDATE 0

/* The controller node class */
class SetPoint: public YarpNode {
    /* Output: setpoint */
    YarpOutput<OBN_PB, double> setpoint;
    
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution;
    
public:
    SetPoint(const std::string& name, const std::string& ws = ""): YarpNode(name, ws), setpoint("sp"),
    generator(std::chrono::system_clock::now().time_since_epoch().count()), distribution(-100, 100)
    { }
    
    /* Add ports to node, hardware components may be started, etc. */
    bool initialize() {
        bool success;
        
        // Add the ports to the node
        if (!(success = addOutput(&setpoint))) {
            std::cerr << "Error while adding setpoint output." << std::endl;
        }
        
        // Open the SMN port
        success = success && openSMNPort();
        
        return success;
    }
    
    /* This node should not receive UPDATE_X. */
    virtual void onUpdateX() {
        std::cout << "At " << _current_sim_time << " UPDATE_X" << std::endl;
    }
    
    /* This callback is called everytime this node's simulation starts or restarts.
     This is different from initialize() above. */
    virtual void onInitialization() {
        // Initial state and output
        setpoint = 0.0;
        std::cout << "At " << _current_sim_time << " INIT" << std::endl;
    }
    
    /* This callback is called when the node's current simulation is about to be terminated. */
    virtual void onTermination() {
        std::cout << "At " << _current_sim_time << " TERMINATED" << std::endl;
    }
    
    /* There are other callbacks for reporting errors, etc. */
    
    /* Declare the update types of the node by listing their index constants in the macro OBN_DECLARE_UPDATES(...)
     Their listing order determines the order in which the corresponding update callbacks are called. */
    OBN_DECLARE_UPDATES(MAIN_UPDATE)
};

/* For each update type, define the update callback function OUTSIDE the class declaration.
 Each callback is defined by OBN_DEFINE_UPDATE(<Your node class name>, <Index of the update type>) { code here; } */

OBN_DEFINE_UPDATE(SetPoint, MAIN_UPDATE) {
    setpoint = distribution(generator) / 10.0;
    std::cout << "At " << _current_sim_time << " UPDATE_Y" << std::endl;
}

int main() {
    std::cout << "This is setpoint node." << std::endl;
    
    SetPoint node("sp", "test2");      // Node "sp" inside workspace "test2"
    if (!node.initialize()) {
        return 1;
    }
    
    // Here we will not connect the node to the GC, let the SMN do it
    
    std::cout << "Starting simulation..." << std::endl;
    
    node.run();
    
    std::cout << "Simulation finished. Goodbye!" << std::endl;
    
    //////////////////////
    // Clean up before exiting
    //////////////////////
    google::protobuf::ShutdownProtobufLibrary();
    
    return node.hasError()?3:0;
}
