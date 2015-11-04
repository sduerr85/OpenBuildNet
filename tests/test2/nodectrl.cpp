/* -*- mode: C++; indent-tabs-mode: nil; -*- */
/** \file
 * \brief Node that implements a simple controller.
 *
 * Uses the node.cpp framework.
 * Requires YARP for communication with SMN, but will use MQTT for ports if it's available.
 *
 * This file is part of the openBuildNet simulation framework
 * (OBN-Sim) developed at EPFL.
 *
 * \author Truong X. Nghiem (xuan.nghiem@epfl.ch)
 */

#include <fstream>
#include <obnnode.h>

/** The following macros are defined by CMake to indicate which libraries this SMN build supports:
 - OBNNODE_COMM_YARP: if YARP is supported for communication.
 - OBNNODE_SMN_COMM_MQTT: if MQTT is supported for communication.
 */

#ifndef OBNNODE_COMM_YARP
#error This test requires YARP to run
#endif

using namespace OBNnode;

#ifdef OBNNODE_COMM_MQTT
#define MQTT_SERVER_ADDRESS "tcp://localhost:1883"
typedef MQTTInput<OBN_PB, double> INPUT_PORT_CLASS;
typedef MQTTOutput<OBN_PB, double> OUTPUT_PORT_CLASS;
MQTTClient mqtt_client;     // The MQTT client of this node (used for all communications)
#else
typedef YarpInput<OBN_PB, double> INPUT_PORT_CLASS;
typedef YarpOutput<OBN_PB, double> OUTPUT_PORT_CLASS;
#endif

#define MAIN_UPDATE 0

/* The controller node class */
class Controller: public YarpNode {
    /* Two inputs: velocity and setpoint */
    /* Output: the control value */
#ifdef OBNNODE_COMM_MQTT
    INPUT_PORT_CLASS velocity{"v", &mqtt_client}, setpoint{"sp", &mqtt_client};
    OUTPUT_PORT_CLASS command{"u", &mqtt_client};
#else
    INPUT_PORT_CLASS velocity{"v"}, setpoint{"sp"};
    OUTPUT_PORT_CLASS command{"u"};
#endif
    
    /* The state variable */
    Eigen::Vector3d x;
    
    /* The state matrix */
    Eigen::Matrix3d A;
    
    /* The output matrix */
    Eigen::RowVector3d C;
    
    std::ofstream dump;
    const char tab = '\t';
    
public:
    Controller(const std::string& name, const std::string& ws = ""): YarpNode(name, ws), dump("controller.txt")
    { }
    
    virtual ~Controller() {
        dump.close();
    }
    
    /* Add ports to node, hardware components may be started, etc. */
    bool initialize() {
        bool success;
        
        // Add the ports to the node
        if (!(success = addInput(&velocity))) {
            std::cerr << "Error while adding velocity input." << std::endl;
        }
        
        if (success && !(success = addInput(&setpoint))) {
            std::cerr << "Error while adding setpoint input." << std::endl;
        }
        
        if (success && !(success = addOutput(&command))) {
            std::cerr << "Error while adding control output." << std::endl;
        }
        
        // Add the update
        success = success && (addUpdate(MAIN_UPDATE, std::bind(&Controller::doMainUpdate, this), std::bind(&Controller::doStateUpdate, this)) >= 0);
        
        // Open the SMN port
        success = success && openSMNPort();
        
        // Initialize the state matrix
        if (success) {
            A <<
            -0.82, 1.0, 0.82,
            1.0 , 0.0, 0.0,
            0.0 , 1.0, 0.0;
            
            C << 12.62, -19.75, 7.625;
        }
        
        return success;
    }
    
    /* Compute the output */
    void doMainUpdate() {
        command = C * x;
        std::cout << "At " << _current_sim_time << " UPDATE_Y" << std::endl;
    }
    
    /* Update the state. */
    void doStateUpdate() {
        // Update the state
        x = A * x;
        x(0) += 32.0 * (setpoint() - velocity());
        
        // At this point, the inputs are all up-to-date, regardless of the order, so we can dump log data
        dump << _current_sim_time << tab << setpoint() << tab << velocity() << tab << command() << tab << x.transpose() << std::endl;
        std::cout << "At " << _current_sim_time << " UPDATE_X" << std::endl;
    }
    
    /* This callback is called everytime this node's simulation starts or restarts.
     This is different from initialize() above. */
    virtual void onInitialization() override {
        // Initial state and output
        x.setZero();
        command = 0.0;
        std::cout << "At " << _current_sim_time << " INIT" << std::endl;
    }
    
    /* This callback is called when the node's current simulation is about to be terminated. */
    virtual void onTermination() override {
        std::cout << "At " << _current_sim_time << " TERMINATED" << std::endl;
    }
    
    /* There are other callbacks for reporting errors, etc. */
};


int main() {
    std::cout << "This is controller node." << std::endl;
    
    // Initialize the MQTT client
#ifdef OBNNODE_COMM_MQTT
    mqtt_client.setServerAddress(MQTT_SERVER_ADDRESS);
    mqtt_client.setClientID("test2_ctrl");
    if (!mqtt_client.start()) {
        std::cerr << "Error while connecting to MQTT" << std::endl;
        return 10;
    }
#endif
    
    Controller ctrl("ctrl", "test2");       // Node "ctrl" inside workspace "test2"

    if (!ctrl.initialize()) {
        return 1;
    }
    
    // Here we will not connect the node to the GC, let the SMN do it
    
    std::cout << "Starting simulation..." << std::endl;

    ctrl.run();
    
    std::cout << "Simulation finished. Goodbye!" << std::endl;
    
    
    //////////////////////
    // Clean up before exiting
    //////////////////////
    if (mqtt_client.isRunning()) {
        mqtt_client.stop();
    }
    
    google::protobuf::ShutdownProtobufLibrary();
    
    return ctrl.hasError()?3:0;
}
