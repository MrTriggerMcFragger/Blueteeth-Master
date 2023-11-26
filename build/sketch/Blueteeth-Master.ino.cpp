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
TaskHandle_t dataStreamPackagerTaskHandle;

terminalParameters_t terminalParameters;
int discoveryIdx;

BluetoothA2DPSink a2dpSink;

BlueteethMasterStack internalNetworkStack(10, &packetReceptionTaskHandle, &Serial2, &Serial1); //Serial1 = Data Plane, Serial2 = Control Plane
BlueteethBaseStack * internalNetworkStackPtr = &internalNetworkStack; //Need pointer for run-time polymorphism

bool streamActive;

/*  Callback for when data is received from A2DP BT stream
*   
*   @data - Pointer to an array with the individual bytes received.
*   @length - The number of bytes received.
*/ 
#line 27 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void a2dpSinkDataReceived(const uint8_t *data, uint32_t length);
#line 40 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void read_data_stream(const uint8_t *data, uint32_t length);
#line 51 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void setup();
#line 99 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void loop();
#line 106 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void ringTokenWatchdogTask(void * params);
#line 143 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void dataStreamPackagerTask(void * params);
#line 177 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void int2Bytes(uint32_t integer, uint8_t * byteArray);
#line 188 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
uint32_t bytes2Int(uint8_t * byteArray);
#line 199 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void packetReceptionTask(void * pvParams);
#line 256 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void terminalInputTask(void * params);
#line 27 "C:\\Users\\ztzac\\Documents\\GitHub\\Blueteeth-Master\\Blueteeth-Master.ino"
void a2dpSinkDataReceived(const uint8_t *data, uint32_t length){
  // Serial.print("BLUETOOTH DATA RECEIVED!");
  
  for (int i = 0; i < length; i++){
    internalNetworkStack.dataBuffer.push_back(data[i]);
  }

  if (streamActive == false){
    vTaskResume(dataStreamPackagerTaskHandle);
    streamActive = true;
  }
}

void read_data_stream(const uint8_t *data, uint32_t length) {
    // process all data
    int16_t *values = (int16_t*) data;
    for (int j=0; j<length/2; j+=2){
      // print the 2 channel values
      Serial.print(values[j]);
      Serial.print(",");
      Serial.println(values[j+1]);
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
  3, // Priority
  &terminalInputTaskHandle); // Task handler
  
  xTaskCreate(ringTokenWatchdogTask, // Task function
  "RING TOKEN WATCHDOG", // Task name
  4096, // Stack depth 
  NULL, 
  2, // Priority
  &ringTokenWatchdogTaskHandle); // Task handler

  xTaskCreate(dataStreamPackagerTask, // Task function
  "DATA STREAM PACKAGER", // Task name
  8192, // Stack depth 
  NULL, 
  1, // Priority
  &dataStreamPackagerTaskHandle); // Task handler

  xTaskCreate(packetReceptionTask, // Task function
  "PACKET RECEPTION HANDLER", // Task name
  4096, // Stack depth 
  NULL, 
  2, // Priority
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

#define SENTINEL_CHAR (0b11111111) 
void inline packDataStream(uint8_t * packedData, int len, deque<uint8_t> & dataBuffer){

    uint8_t select_lower;
    size_t packagedDataEnd = len + len/7*2; //For each 7 bytes, 1 sentinal character byte and 8 0 bits are added.
    
    Serial.printf("Buffer size before is %d\n\r", dataBuffer.size());
    for(int frame = 0; frame < packagedDataEnd; frame += 9){
        select_lower = 0b00000001; //used to select the lower portion of the unpacked byte;
        
        packedData[frame] = SENTINEL_CHAR;
        packedData[frame + 1] = 0; //Need to set the first actual packaged byte to 0 for loop to work
        for(int byte = 1; byte < 8; byte++){
            packedData[frame + byte] += dataBuffer.front() >> byte;
            packedData[frame + byte + 1] = (select_lower & dataBuffer.front()) << (7 - byte); 
            dataBuffer.pop_front();

            select_lower = (select_lower << 1) + 1;
        }
        // Serial.printf("Frame %d filled\n\r", frame);
    }
    Serial.printf("Buffer size after is %d\n\r", dataBuffer.size());

}

void dataStreamPackagerTask(void * params) {

  while (1){

    if (internalNetworkStack.dataBuffer.size() == 0) { 
      streamActive = false;
      vTaskSuspend(NULL);
    }

    size_t dataLen = min(internalNetworkStack.dataBuffer.size(), (size_t) DATA_PLANE_SERIAL_TX_BUFFER_SIZE); 
    //When receiving a random number of bytes, will need to add up to 7 additional zeros to make the entire payload divisible by 7
    if ((dataLen % 7) != 0){
      int inc = 7 - (dataLen % 7);
      // Serial.printf("Data length is %d bytes, need to add %d bytes", dataLen, inc);
      for (int i = 0; i < (dataLen + inc); i++){ 
        internalNetworkStack.dataBuffer.push_back(0);
      }
      dataLen += inc;
    }

    size_t frameLen = dataLen + dataLen/7*2; Serial.printf("Data length is %d and frame length is %d\n\r", dataLen, frameLen);
    uint8_t tmp[frameLen]; Serial.print("Formatted data stream... ");
    packDataStream(tmp, dataLen, internalNetworkStack.dataBuffer); Serial.print("Packed data stream... ");
    Serial.printf("Buffer size is %d ", internalNetworkStack.dataBuffer.size());
    internalNetworkStack.streamData(tmp, frameLen); Serial.print("Sent data stream...\n\r");
  }
}

/*  Gets individual bytes of a 32 bit integer
*   
*   @integer - the integer being analyzed
*   @byteArray - array containing 4 bytes corresponding to a 32 bit integer
*   @return - the resulting integer
*/  
inline void int2Bytes(uint32_t integer, uint8_t * byteArray){
  for (int offset = 0; offset < 32; offset += 8){
    byteArray[offset/8] = integer >> offset; //assignment will truncate so only first 8 bits are assigned
  }
}

/*  Unpacks byte array into a 32 bit integer
*   
*   @byteArray - array containing 4 bytes corresponding to a 32 bit integer
*   @return - the resulting integer
*/  
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

/*  Prints all characters in a character buffer
*
*   @endPos - last buffer position that should be printed
*/ 
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
          
          case CONNECT:
            newPacket.type = CONNECT;
            for (int address = 1; address <= 2; address++){
              newPacket.dstAddr = address;
              sprintf((char *) newPacket.payload, "Wireless Speaker");
              internalNetworkStack.queuePacket(1, newPacket);
            }
            break;
          
          case DROP:
            internalNetworkStack.dataBuffer.resize(0);
            // newPacket.type = DROP;
            // internalNetworkStack.queuePacket(1, newPacket);
            break;
          
          case DISCONNECT:
            newPacket.dstAddr = 1;
            newPacket.type = DISCONNECT;
            internalNetworkStack.queuePacket(1, newPacket);
            break;

          case PING:
            newPacket.type = PING;
            internalNetworkStack.queuePacket(1, newPacket);
            break;

          case INITIALIZAITON:
            newPacket.dstAddr = 255;
            newPacket.type = INITIALIZAITON;
            newPacket.payload[0] = 1;
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

            uint32_t t = millis();

            // uint8_t streamArray[255];
            // for (int i = 0; i < 255; i++){
            //     streamArray[i]=i+1;
            // }
            // uint8_t cnt = 0;
            // while (cnt < 157) {
            //   if (cnt == 156){
            //     internalNetworkStack.streamData(streamArray, 220);
            //   }
            //   else {
            //     internalNetworkStack.streamData(streamArray, 255);
            //   }
            //   cnt++;
            // }

            for (int i = 0; i < 40000; i++){

              internalNetworkStack.dataBuffer.push_back( (i % 255) + 1 );
            
            }

            if (streamActive == false) {
              vTaskResume(dataStreamPackagerTaskHandle);
              streamActive = true;
            }

            t = millis() - t;

            Serial.printf("40 kByte transmission finished in %d milliseconds\n\r", t);
            
            delay(100);

            BlueteethPacket streamRequest(false, internalNetworkStack.getAddress(), 254);
            streamRequest.type = STREAM;
            internalNetworkStack.queuePacket(true, streamRequest);

            break;

          }

          case TEST:
            Serial.print("Attempting to stream sample audio data on the data plane\n\r");
            internalNetworkStack.streamData((uint8_t *) piano16bit_raw, sizeof(piano16bit_raw));
            
            // Serial.print("Printing out samples to terminal\n\r");
            // a2dpSink.set_stream_reader(read_data_stream);
            break;
            
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
