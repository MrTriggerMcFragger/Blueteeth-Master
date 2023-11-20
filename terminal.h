#define MAX_BUFFER_SIZE (100)
#define MAX_ARGS (4)
#define NUM_PERSISTENT_LINES 8

#include "BlueteethInternalNetworkStack.h"

typedef struct {
  int scanIdx;
} terminalParameters_t;

//Name: format_terminal_for_new_entry
//Purpose: 
//Inputs: 
//Outputs: 
inline void format_terminal_for_new_entry(int numEntries){
  for (int i = 0; i < numEntries; i++){
    Serial.print("\0337"); //save cursor positon
    Serial.printf("\033[%dF", numEntries);
  }
}

//Name: format_terminal_for_new_entry
//Purpose: 
//Inputs: 
//Outputs: 
inline void format_new_terminal_entry(){
    Serial.print("\0338"); //restore cursor position
}

//Name: clear_buffer
//Purpose: Replace all values in an array with null terminators. 
//Inputs: buffer (a pointer to the character buffer) and buffer_entries (an integer value containing the number of characters in the buffer)  
//Outputs: None
inline void clear_buffer(char * buffer, int buffer_entries){
    for (int idx = 0; idx < buffer_entries; idx++){
        buffer[idx] = '\0'; //Fill w/ null terminators
    }
}

//Name: argument_mapping
//Purpose: map arguments to corresponding actions.
//Inputs: char * arguments (array of pointers to argument strings), num_args (the number of actual arguments received)  
//Outputs: action (the action that should be performed based on argument mapping)
PacketType argument_mapping(char * arguments[MAX_ARGS], uint8_t num_args, terminalParameters_t & terminalParameters){

    if (0 == strcmp(arguments[0], "connect")){ 
      format_terminal_for_new_entry(1);
      Serial.print("Initiating connecitons\n\r");
      format_new_terminal_entry();
      return CONNECT;
    }

    else if (0 == strcmp(arguments[0], "init")){ 
      format_terminal_for_new_entry(1);
      Serial.print("Re-initializing\n\r");
      format_new_terminal_entry();
      return INITIALIZAITON;
    }

    else if (0 == strcmp(arguments[0], "help")){ 
      format_terminal_for_new_entry(1);
      Serial.print("Valid options are: clear and scan.\n\r");
      format_new_terminal_entry();
    }

    else if (0 == strcmp(arguments[0], "ping")){ 
      format_terminal_for_new_entry(1);
      Serial.print("Starting ping\n\r");
      format_new_terminal_entry();
      return PING;
    }

    else if (0 == strcmp(arguments[0], "scan")){ 
      format_terminal_for_new_entry(1);
      Serial.print("Scan starting\n\r");
      format_new_terminal_entry();
      return SCAN;
    }

    else if (0 == strcmp(arguments[0], "select")){ 
      if (num_args < 2) Serial.print("Incorrect number of arguments for select command");
      else {

        terminalParameters.scanIdx = atoi(arguments[1]);
        format_terminal_for_new_entry(1);
        Serial.print("Selected\n\r");
        format_new_terminal_entry();
        return SCAN;

      }
    }
    
    else if (0 == strcmp(arguments[0], "stream")){ 
      return STREAM;
    }
    
    else if (0 == strcmp(arguments[0], "test")){ 
      return TEST;
    }

    else if (0 == strcmp(arguments[0], "clear")){
      Serial.print("\033[H");
      Serial.printf("\33[2J");      
    }
    
    else{
      format_terminal_for_new_entry(1);
      Serial.print("Invalid entry. Type help to find out what options are available.\n\r");
      format_new_terminal_entry();
    }

    return NONE;
}


//Name: handle_input
//Purpose: handle the user's input.
//Inputs: user_input (C string containing the user input)
//Outputs: action (enum corresponding to action to be performed)
PacketType handle_input(char * user_input, terminalParameters_t & terminalParameters){
    
    const char delimiter[2] = " ";
    char * arguments[MAX_ARGS];
    size_t arg_num = 0; 

    //It seems as though I ran into a legitimate compiler bug as the conditional in the block below legitimately doesn't get evaluated correctly. 
    //Print statements tell me that arg_num is 8, but it thinks arg_num is also less than 4. Either program memory is being overwritten, 
    //or I've run into some edgecase. Compiling this snippet using repl tokenizes correctly, and the conditional is evaluated
    //correctly.

    // while(arg_num < 4){ //parse arguments
    //     if (arguments[arg_num] == NULL){
    //         Serial.printf("No additional arguments found...");
    //         break;
    //     }
    //     arguments[arg_num++] = strtok(NULL, delimiter);
    //     Serial.printf("Argument %d is %s [Sanity Check = %d]", arg_num, arguments[arg_num], arg_num<4);
    // }
    
    //I spent over an hour banging my head against a wall, so instead
    //I'm just gonna call strtok four times, and check for null pointers
    //that way we don't access memory we're not supposed to. 
    arguments[arg_num++] = strtok(user_input, delimiter);    
    arguments[arg_num++] = strtok(NULL, delimiter);
    arguments[arg_num++] = strtok(NULL, delimiter);
    arguments[arg_num] = strtok(NULL, delimiter); 
    
    uint8_t num_args = 0;
    if (arguments[arg_num--] != NULL) num_args++;
    if (arguments[arg_num--] != NULL) num_args++;
    if (arguments[arg_num--] != NULL) num_args++;
    if (arguments[arg_num] != NULL) num_args++;

    return argument_mapping(arguments, num_args, terminalParameters);

}