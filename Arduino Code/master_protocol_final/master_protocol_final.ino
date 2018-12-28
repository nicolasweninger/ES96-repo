// include external libs for nrf24l01+ radio transceiver communications
#include <RF24.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <printf.h>

// set Chip-Enable (CE) and Chip-Select-Not (CSN) radio setup pins
#define CE_PIN 9
#define CSN_PIN 10

// defining node number array length 
#define node_no 40

// create RF24 radio object using selected CE and CSN pins
RF24 radio(CE_PIN,CSN_PIN);

// Defining all nodes
char nodeAddresses[node_no] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0xA,0xB,0xC,0xD,0xE,0xF,0x10,0x11,0x12};

//Defining bool array of available nodes
bool nodeAvailable[node_no] = {1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// Defining zero pipe
char zeroPipe = 0x00;

// Defining decommission array
byte overwrite[4] = {'0','x','f','f'};

// int array to write to slave {age, alarm state}
typedef union writeArrayT {
  struct {
    unsigned long age;
    bool alarmstate;
  } asStruct;
  byte asbytes[5];
} writeArrayT;

// Declare the global array
writeArrayT writeArray;

// float array for our data {elapsed time, HR, RR, Temp}
typedef union sensorDataT {
  struct {
    unsigned long stime;
    float HR;
    float RR;
    float temp;
  } asvalues;
  byte asbytes[16];
} sensorDataT;

// Defining the global array
sensorDataT sensorData;

// marker for the next available node
int lowestAbsentNode = 1;

// Execute set up once
void setup()
{
    writeArray.asStruct.age = 28;

    // setup serial communications for basic program display
    Serial.begin(9600);
    Serial.println("[*][*][*] Beginning nRF24L01+ master-multiple-slave program [*][*][*]");
    
    // ----------------------------- RADIO SETUP CONFIGURATION AND SETTINGS -------------------------//
    
    // begin radio object
    radio.begin();
    
    // set power level of the radio
    radio.setPALevel(RF24_PA_LOW);
    
    // set RF datarate - lowest rate for longest range capability
    radio.setDataRate(RF24_250KBPS);
    
    // set radio channel to use - ensure all slaves match this
    radio.setChannel(0x76);
    
    // set time between retries and max no. of retries
    radio.setRetries(4, 10);
    
    // enable ack payload - each slave replies with sensor data using this feature
    radio.enableAckPayload();
    printf_begin();
    radio.printDetails();

    // --------------------------------------------------------------------------------------------//
}



void loop()
{
  // Look for preexisting devices. To be removed in final version
  checkActiveDevices();
  
  // Stay in this loop
  while (true) {
    // First scan the pre-existing patients for data
    checkPatientData();
    // Delay for sanity. To be removed in the end
    delay(3000);
    // After scanning, look for new devices
    // TO BE IMPLEMENTED IN IF LOOP UPON BUTTON CLICK IN THE FINAL VERSION
    newDeviceProtocol();
  }
  // Decomm code
  // TO BE IMPLEMENTED IN IF LOOP UPON BUTTON CLICK IN THE FINAL VERSION
  decommissionDevice(1);
}

// Run once at startup; check for any pre-existing active devices\
// To be edited once we have the bool array saved on the pi
void checkActiveDevices(void) {
  for (int a=0; a<node_no; a++) {
    radio.openWritingPipe(nodeAddresses[a]);
        
    //boolean to indicate if radio.write() tx was successful
    bool tx_sent = radio.write(&writeArray.asbytes, sizeof(writeArray.asbytes));
    // if tx success - receive and read slave node ack reply
    if (tx_sent) {
      Serial.print("Device found at node ");
      Serial.println(a+1);      
      if (radio.isAckPayloadAvailable()) {
        if (nodeAvailable[a] == 0) {
          nodeAvailable[a] = 1;
          Serial.print("Device found at node ");
          Serial.println(a+1);
        }
      }
    }
  }
}

// Called once per cycle. Attempts to listen for data from patient for x seconds (?)
void checkPatientData()
{       
  // make a call for data to each node in turn
  for (int node = 0; node < node_no; node++) {
    if (nodeAvailable[node]==1 && node!=0 && node!=1) {
      // If the node exists in the array, then scan for it for y seconds
      pullPatientData(node);
    }
  }
  newDeviceProtocol();
}

// Scans that particular node for data for y seconds
void pullPatientData(int k) {
  
  Serial.print("attempting to get data from node:");
  Serial.println(k+1);
  // Communicate along that node pipe
  radio.openWritingPipe(nodeAddresses[k]);
  writeArray.asStruct.alarmstate = 0;
  //boolean to indicate if radio.write() tx was successful
  bool tx_sent = radio.write(&writeArray.asbytes, sizeof(writeArray.asbytes));
        
  // if tx success - receive and read slave node ack reply
  if (tx_sent) {
    Serial.print("We are sending the array!");  
    writeArray.asStruct.age++;

    if (radio.isAckPayloadAvailable()) {
                  
      //read ack payload and copy data to relevant remoteNodeData array
      radio.read(&sensorData.asbytes, sizeof(sensorData.asbytes));
                    
      //Serial.print("Node ");
      Serial.print(k+1);
      //Serial.print(". Time elapsed: ");
      Serial.print(",");
      Serial.print(sensorData.asvalues.stime);
      //Serial.print(". Heartrate: ");
      Serial.print(",");
      Serial.print(sensorData.asvalues.HR);
      //Serial.print(". Respiratory rate: ");
      Serial.print(",");
      Serial.print(sensorData.asvalues.RR);
      //Serial.print(". Temperature: ");
      Serial.print(",");
      Serial.println(sensorData.asvalues.temp);
    }
  }
}
  
// To be executed on button click
void newDeviceProtocol() {
  
  // initializing timer
  int rewritetime = millis();
  
  // Defining success char for zero pipe overwrite
  char rewrite = 0x00;
  bool rewritebool = false;
  
  //while (millis()-rewritetime <30000 && rewritebool==false)
  
  // Opening zero pipe
  radio.openWritingPipe(zeroPipe);

  for (int n=0; n<node_no; n++) {
 
    if (nodeAvailable[n]==0) {
      lowestAbsentNode = n+1;
      n = node_no;
    }
  }
  
  // Attempt to write to the 00 channel
  bool newNode = radio.write(&lowestAbsentNode,sizeof(lowestAbsentNode));

  //Checking for successful overwrite
  radio.openReadingPipe(1,nodeAddresses[lowestAbsentNode-1]);
  radio.openWritingPipe(nodeAddresses[lowestAbsentNode-1]);  
  bool tx_sent = radio.write(&writeArray, sizeof(writeArray));
        
  // if tx success - receive and read slave node ack reply
  if (tx_sent) {
    if (radio.isAckPayloadAvailable()) {      
      //read ack payload and copy data to relevant remoteNodeData array
      radio.read(&rewrite,sizeof(rewrite));
      
      if (rewrite = 0xff) {
        Serial.print("New node: ");
        Serial.println(lowestAbsentNode);
        
        // Setting the node as unavailable once there is a single successful connection
        nodeAvailable[lowestAbsentNode-1] = 1;
        
        // Quitting the loop if success
        rewritebool=true;
      }
    }
  }
  if (rewritebool==false) {
   Serial.println("Node assignment failed.");
  } 
}

void decommissionDevice(int nodekill) {
  radio.openWritingPipe(nodeAddresses[nodekill-1]);
  
  bool tx_sent;
  tx_sent = radio.write(&overwrite, sizeof(overwrite));
  
  Serial.print("[*] Attempting to rewrite node ");
  Serial.println(int(nodekill)-1);
  
  radio.openReadingPipe(1,zeroPipe);
  
  
  char successfulOverwrite;
  if (tx_sent) {
    if (radio.isAckPayloadAvailable()) {
      radio.read(&successfulOverwrite, sizeof(successfulOverwrite));
      if (successfulOverwrite == 0xf1) {
        Serial.print("Success! Node ");
        Serial.print(int(nodekill)-1);
        Serial.println(" has been killed.");
        nodeAvailable[nodekill-1]=0;
      }
      else { 
        Serial.println("Rewrite unsuccessful");
      }
    }
  }
}

