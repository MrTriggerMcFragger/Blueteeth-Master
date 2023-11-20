#include <Arduino.h>
#line 1 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
#include "Blueteeth-Master.h"

int scanTime = 5; //In seconds
char input_buffer[MAX_BUFFER_SIZE];
BLEScan* pBLEScan;
SemaphoreHandle_t uartMutex;
TaskHandle_t terminalInputTaskHandle;
TaskHandle_t ringTokenWatchdogTaskHandle;
TaskHandle_t packetReceptionTaskHandle;

terminalParameters_t terminalParameters;
int discoveryIdx;

BluetoothA2DPSink a2dpSink;

BlueteethMasterStack internalNetworkStack(10, &packetReceptionTaskHandle, &Serial2, &Serial1); //Serial1 = Data Plane, Serial2 = Control Plane
BlueteethBaseStack * internalNetworkStackPtr = &internalNetworkStack; //Need pointer for run-time polymorphism

uint32_t streamTime; //TEMPORARY DEBUG VARIABLE (REMOVE LATER)

#line 21 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void a2dpSinkDataReceived(const uint8_t *data, uint32_t length);
#line 28 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void setup();
#line 69 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void loop();
#line 76 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void ringTokenWatchdogTask(void * params);
#line 88 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void int2Bytes(uint32_t integer, uint8_t * byteArray);
#line 94 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
uint32_t bytes2Int(uint8_t * byteArray);
#line 105 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void packetReceptionTask(void * pvParams);
#line 158 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void terminalInputTask(void * params);
#line 21 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void a2dpSinkDataReceived(const uint8_t *data, uint32_t length){
  // Serial.print("BLUETOOTH DATA RECEIVED!");
  for (int i = 0; i < length; i++){
    internalNetworkStack.streamData(*(data + i));
  }
}

void setup() {
  
  //Start Serial comms
  Serial.begin(115200);
  uartMutex = xSemaphoreCreateMutex(); //mutex for UART

  internalNetworkStack.begin();
  
  //Setup Peripherals
  // pBLEScan = bleScanSetup();

  //State variable initialization
  terminalParameters.scanIdx = -1;

  //Create tasks
  xTaskCreate(terminalInputTask, // Task function
  "UART TERMINAL INPUT", // Task name
  4096, // Stack depth
  NULL, 
  1, // Priority
  &terminalInputTaskHandle); // Task handler
  
  xTaskCreate(ringTokenWatchdogTask, // Task function
  "RING TOKEN WATCHDOG", // Task name
  4096, // Stack depth 
  NULL, 
  1, // Priority
  &ringTokenWatchdogTaskHandle); // Task handler

  xTaskCreate(packetReceptionTask, // Task function
  "PACKET RECEPTION HANDLER", // Task name
  4096, // Stack depth 
  NULL, 
  1, // Priority
  &packetReceptionTaskHandle); // Task handler

  a2dpSink.set_stream_reader(a2dpSinkDataReceived);
  a2dpSink.set_auto_reconnect(false);
  a2dpSink.start("Blueteeth Sink"); //Begin advertising
}

void loop() {
}


/*  Checks to see if the ring token is still in the network. If it isn't detected after some period, generates a new token.
*
*/  
void ringTokenWatchdogTask(void * params) {
  while (1){
    vTaskDelay(RING_TOKEN_GENERATION_DELAY_MS);
    if (internalNetworkStack.getTokenRxFlag() == false){
      Serial.print("Generating a new token.\n\r"); //DEBUG STATEMENT
      // internalNetworkStack.tokenReceived();
      internalNetworkStack.generateNewToken();
    }
    internalNetworkStack.resetTokenRxFlag(); 
  }
}

inline void int2Bytes(uint32_t integer, uint8_t * byteArray){
  for (int offset = 0; offset < 32; offset += 8){
    byteArray[offset/8] = integer >> offset; //assignment will truncate so only first 8 bits are assigned
  }
}

inline uint32_t bytes2Int(uint8_t * byteArray){
  uint32_t integer = 0;
  for (int offset = 0; offset < 32; offset += 8){
    integer += byteArray[offset/8] << offset; 
  }
  return integer;
}

/*  Task that runs when a new Blueteeth packet is received. 
*
*/  
void packetReceptionTask (void * pvParams){
  while(1){
    
    vTaskSuspend(packetReceptionTaskHandle);
    BlueteethPacket packetReceived = internalNetworkStack.getPacket();

    switch(packetReceived.type){
      
      case PING:
        Serial.print("Ping packet type received.\n\r"); //DEBUG STATEMENT
        Serial.printf("Response from address %s\n\r", packetReceived.payload);
        break;
      
      case STREAM_RESULTS:
        Serial.printf("Stream results from ADDR%d: Checksum = %d, Time = %d\n\r", packetReceived.srcAddr, bytes2Int(packetReceived.payload), bytes2Int(packetReceived.payload + 4));
        break;

      default:
        Serial.print("Unknown packet type received.\n\r"); //DEBUG STATEMENT
        break;
    }

  }
}

void inline printBuffer(int endPos){

  Serial.print("\0337"); //save cursor positon
  Serial.printf("\033[%dF", endPos + 1); //go up N + 1 lines
  for (int i = 0; i <= endPos; i++) {

    Serial.print("\033[2K"); //clear line
    
    switch(input_buffer[i]){
      case '\0':
        Serial.printf("Character %d = NULL\n\r", i);
        break;
      case '\n':
        Serial.printf("Character %d = NEWLINE\n\r", i);
        break;
      case 127:
        Serial.printf("Character %d = BACKSPACE\n\r", i);
        break;
      default:
        Serial.printf("Character %d = %c\n\r", i , input_buffer[i]);
    }
  }
  Serial.print("\0338"); //restore cursor position
}

/*  Take in user inputs and handle pre-defined commands.
*
*/
void terminalInputTask(void * params) {

  clear_buffer(input_buffer, sizeof(input_buffer));
  int buffer_pos = 0;
  BLEScanResults scanResults;
  const char * btTarget;
  
  while(1){

    vTaskDelay(100);

    xSemaphoreTake(uartMutex, portMAX_DELAY);

    while (Serial.available() && (buffer_pos < MAX_BUFFER_SIZE)){ //get number of bits on buffer
      
      input_buffer[buffer_pos] = Serial.read();

      //handle special chracters
      if (input_buffer[buffer_pos] == '\r') { //If an enter character is received
        
        input_buffer[buffer_pos] = '\0'; //Get rid of the carriage return
        Serial.print("\n\r");
        
        BlueteethPacket newPacket (false, internalNetworkStack.getAddress(), 254); //Need to declare prior to switch statement to avoid "crosses initilization" error.

        switch ( handle_input(input_buffer, terminalParameters) ){
          case PING:
            newPacket.type = PING;
            internalNetworkStack.queuePacket(1, newPacket);
            break;

          case SCAN:
            
            discoveryIdx = 0;

            scanResults = performBLEScan(pBLEScan, 5);
            vTaskDelay(5 * 1000);
            
            Serial.print("\0337"); //save cursor
            Serial.printf("\033[%dF", scanResults.getCount() + 1); //go up N + 1 lines
            Serial.print("\0332K"); //clear line
            Serial.print("*** SCAN RESULTS START ***");
            Serial.print("\0338"); //restore cursor
            Serial.print("*** SCAN RESULTS END   ***\n\r");

            break;

          case SELECT:
            if ((terminalParameters.scanIdx > 0) && (terminalParameters.scanIdx < discoveryIdx)){
              btTarget = scanResults.getDevice(terminalParameters.scanIdx).getName().c_str(); 
              Serial.printf("Target set to %s\n\r", btTarget);
            }
            else {
              Serial.print("Selection failed\n\r");
            }
            break;

          case STREAM : {
            for (int j = 0; j < 156; j++){
              for (int i = 1; i <= 255; i++){
                internalNetworkStack.streamData(i);
              }
            }
            for (int i = 0; i < 220; i++){
              internalNetworkStack.streamData(i);
            }

            BlueteethPacket streamRequest(false, internalNetworkStack.getAddress(), 254);
            streamRequest.type = STREAM;
            internalNetworkStack.queuePacket(true, streamRequest);
            break;
          }
            
          default:
            break;
            //no action needed

        } //handle the input
        clear_buffer(input_buffer, sizeof(input_buffer));
        buffer_pos = -1; //return the buffer back to zero (incrimented after this statement)
        // Serial.printf("Buffer pos is %d", buffer_pos);
      }
      
      else if (input_buffer[buffer_pos] == 127){ //handle a backspace character
        Serial.printf("%c", 127); //print out backspace
        input_buffer[buffer_pos] = '\0'; //clear the backspace 
        if (buffer_pos > 0) input_buffer[--buffer_pos] = '\0'; //clear the previous buffer pos if there was another character in the buffer that wasn't a backspace
        buffer_pos--;
      }
      
      else Serial.printf("%c", input_buffer[buffer_pos]);

      buffer_pos++;
      
    }

    xSemaphoreGive(uartMutex);

  }
}
