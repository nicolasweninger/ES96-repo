// nRF24L01 radio transceiver external libraries
#include <SPI.h>
#include <RF24.h>
#include <nRF24L01.h>
#include <printf.h>

// chip select and RF24 radio setup pins
#define CE_PIN 9
#define CSN_PIN 10
RF24 radio(CE_PIN,CSN_PIN);

// setup radio pipe addresses for each slave node
char nodeAddress = 0x00;
char newNodeID;

// array type to collect from master {age, alarm state}
typedef union writeArrayT {
  struct {
    unsigned long age;
    bool alarmstate;
  } asStruct;
  byte asbytes[5];
} writeArrayT;

// global write array
writeArrayT writeArray;

typedef union sensorarray{
  struct {
    unsigned long stime;
    float HR;
    float RR;
    float temp;
  } asvalues;
  byte asbytes[16];
} sensorarray;

sensorarray sensorData;
  
/* Function: setup
 *    Initialises the system wide configuration and settings prior to start
 */
 
void setup() {

  // setup serial communications for basic program display
  Serial.begin(9600);
  Serial.println("[*][*][*] Beginning nRF24L01+ ack-payload slave device program [*][*][*]");

  // ----------------------------- RADIO SETUP CONFIGURATION AND SETTINGS -------------------------// 

  sensorData.asvalues.stime=2000;
  sensorData.asvalues.HR=70;
  sensorData.asvalues.RR=14;
  sensorData.asvalues.temp=37.4;
  
  radio.begin();
  
  // set power level of the radio
  radio.setPALevel(RF24_PA_LOW);

  // set RF datarate
  radio.setDataRate(RF24_250KBPS);

  // set radio channel to use - ensure it matches the target host
  radio.setChannel(0x76);

  // open a reading pipe on the chosen address for selected node
  radio.openReadingPipe(1, nodeAddress);     

  // enable ack payload - slave reply with data using this feature
  radio.enableAckPayload();

  // preload the payload with initial data - sent after an incoming message is read
  radio.writeAckPayload(1,&sensorData.asbytes, sizeof(sensorData.asbytes));

  // print radio config details to console
  printf_begin();
  radio.printDetails();

  // start listening on radio
  radio.startListening();
  
  // --------------------------------------------------------------------------------------------//
}

void loop() {
  
  // transmit current preloaded data to master device if message request received
  bool works = radioCheckAndReply(sensorData.asbytes,sizeof(sensorData.asbytes));

  if (works==true) {
    Serial.print("We had a connection");
    sensorData.asvalues.stime++;
    sensorData.asvalues.HR++;
    sensorData.asvalues.RR++;
    sensorData.asvalues.temp=37.4;
  }
}


// Runs after the sensor code for a max total time of 10 mins
bool radioCheckAndReply(byte* sendbytes, short sendsize) {

  // Check to see if node is ready to be set
  if (nodeAddress==0x00) {
    bool newnode = checkNodeSet();
    if (newnode) {
      return true;
    }
    else{
      return false;
    }
  }

  else {
    if (radio.available()){
      radio.read(writeArray.asbytes,sizeof(writeArray.asbytes));
      // Check for overwrite procedure first
      if (writeArray.asStruct.age == 0 && writeArray.asStruct.alarmstate == true) {
        nodeAddress = 0x00;
        radio.openReadingPipe(1,nodeAddress);
        radio.openWritingPipe(nodeAddress);
        if (radio.available()) {
          char success = 0xf1;
          radio.writeAckPayload(1,&success,sizeof(success));
          Serial.print("THIS IS THE THING BEING PASSED INTO THE REWRITTEN PIPE ");
          Serial.println(success);
          Serial.println("Our device node address has been successfully overwritten back to the zero pipe");
        }
      }
      // If overwrite procedure not activated
      else {
        Serial.print("The patient's age is: ");
        Serial.print(writeArray.asStruct.age);
        Serial.print(". Our alarm state is: ");
        if (writeArray.asStruct.alarmstate==false) {
          Serial.println("no alarm. ");
        }
        else {
          Serial.println("ALARM! ");
        }
        radio.writeAckPayload(1,sendbytes,sendsize);
        return(true);
      }
    }
    else {
      return(false);
    }
  }
}

bool checkNodeSet(void) {
  // First check for new device protocol
  // marker to indicate successful rewrite of slave from 0 pipe
  Serial.println("Trying to get node assignment.");
  char rewrite = 0xff;
  // Check if the master has a 00 pipe open
  if (radio.available()) {
    // Pull the master's data (should be new node number)
    radio.read(&newNodeID, sizeof(newNodeID));
    // save to be stored by child
    nodeAddress = newNodeID;
    // Print new node
    Serial.print("The new node address of this device is: ");
    Serial.println(nodeAddress,DEC);
  }
  // Open the new node permanently as the reading pipe
  radio.openReadingPipe(1,nodeAddress);
  radio.openWritingPipe(nodeAddress);
  int rewritetime = millis();
  radio.writeAckPayload(1,&rewrite,sizeof(rewrite));

  if (nodeAddress != 0x00) {
    return true;
  }
  else {
    return false;
  }
}
