#include <stdatomic.h>

#include "pico/time.h"

#include "Sim800.h"


// AT command results
enum AT_RESULT_CODES{
    OK = 0,
    ERROR = 1,
    INVALID = 2,
    PROCESSING = 3
};

// SIM800 response data
typedef struct{
    char data[5][50];
    uint8_t size[5];
    uint8_t num_words;
} Sim800ResponseData;

// SMS response
static Sim800ResponseData smsResponse = {{{}}, {},0};

// Generic Sim800 payload
static Sim800ResponseData genericResponse = {{{}}, {},0};

// AT command response
struct{
    Sim800ResponseData payload;
    atomic_int error; 
} atCommandResponse = {{{{}}, {},0}, (enum AT_RESULT_CODES)PROCESSING};


// AT command response handling flags
struct{
    bool is_there_pre_char;
    uint8_t word_count;
    uint8_t char_index;
} atResponseHandlingFlags = {false, 0, 0};

// List of AT commands to setup
const char *startup_at_commands[] = {
    "AT\r\n",
    "AT+CPIN?\r\n",
    "AT+CSQ\r\n",
    "AT+CCID\r\n",
    "AT+CREG?\r\n",
    "AT+CBC\r\n",
    "AT+COPS?\r\n",
    "AT+CMGF=1\r\n",
    "AT+CNMI=1,2,0,0,0\r\n"
};

// List of AT commands to send sms
char endChar = 0x1A;
const char *sms_at_commands[] = {
    "AT+CMGS=\"+94764893779\"\r\n",
    "",
    &endChar,
};

// uart config
UartConfig *gsm_uart_config = NULL;


/*************************Private methods**************************/

// Function to print the updated characters in the buffer
static void print_serial_data(const char *name, char *rx_buff, int n_updated) {
    printf("%s: ", name);
    for (int i = 0; i < n_updated; i++) {
        printf("%c", rx_buff[i]);
    }
    printf("\n");
}

// Function to clear the buffer by setting it to null characters
static void clear_buffer(char *buffer, size_t size) {
    memset(buffer, 0, size);  // Set all bytes of the buffer to 0 (null character)
}

// Function to copy data from one Sim800ResponseData to another
static void copy_sim800_response_data(Sim800ResponseData* dest, const Sim800ResponseData* src) {
    // Copy the num_words value (number of valid strings)
    dest->num_words = src->num_words;

    // Copy the size array (sizes of each string in data)
    memcpy(dest->size, src->size, sizeof(src->size));

    // Copy only the valid strings based on num_words
    for (int i = 0; i < src->num_words; i++) {
        // Copy each string's content up to the size indicated in the size array
        memcpy(dest->data[i], src->data[i], src->size[i]);
        // Null-terminate the string in case it isn't already null-terminated
        dest->data[i][src->size[i]] = '\0';
    }
}

static void cb1(enum AT_RESULT_CODES error){

    //update at command response body
    copy_sim800_response_data(&(atCommandResponse.payload), &genericResponse);

    atCommandResponse.error = error;

    // set latest command result to code
    for(uint8_t i =0; i<atCommandResponse.payload.num_words; i++){
        print_serial_data("Received data", atCommandResponse.payload.data[i], atCommandResponse.payload.size[i] );
    }
}

static void cb2(){
    //update sms body
    copy_sim800_response_data(&(smsResponse), &genericResponse);

    // set latest command result to code
    // for(uint8_t i =0; i<smsResponse.num_words; i++){
    //     print_serial_data("Received SMS data", smsResponse.data[i], smsResponse.size[i] );
    // }
    
}

// Reset uart AT command handling flags
static void reset_uart_handling_params(){
    atResponseHandlingFlags.is_there_pre_char = false;
    atResponseHandlingFlags.word_count = 0;
    atResponseHandlingFlags.char_index = 0;

    for(uint8_t i =0;i<5;i++){
        clear_buffer(genericResponse.data[i],50);
        genericResponse.num_words = 0;
        genericResponse.size[i] = 0;
    }
}

// Reset uart AT command handling flags
static void reset_at_command_reponse(){
    for(uint8_t i =0;i<5;i++){
        clear_buffer(atCommandResponse.payload.data[i],50);
        atCommandResponse.payload.num_words = 0;
        atCommandResponse.payload.size[i] = 0;
    }
    atCommandResponse.error = (enum AT_RESULT_CODES) PROCESSING;
}

// Reset uart AT command response body
static void reset_smsResponse_reponse(){
    for(uint8_t i =0;i<5;i++){
        clear_buffer(smsResponse.data[i],50);
        smsResponse.num_words = 0;
        smsResponse.size[i] = 0;
    }
}

// RX handler which Extract words from the responses OK or ERROR
static void on_uart_rx(){
        char char_in = uart_getc(gsm_uart_config->uart);

        // valid character
        if(char_in != '\n' && char_in !='\r' && char_in != 0){
            // starting letter or mid letter
            if(!(atResponseHandlingFlags.is_there_pre_char)){
                atResponseHandlingFlags.char_index = 0;
                atResponseHandlingFlags.word_count++;
            }

            // Update payload
            genericResponse.data[atResponseHandlingFlags.word_count-1][atResponseHandlingFlags.char_index] = char_in;
            genericResponse.size[atResponseHandlingFlags.word_count-1] = atResponseHandlingFlags.char_index+1;

            // Update flags
            atResponseHandlingFlags.char_index++;
            atResponseHandlingFlags.is_there_pre_char = true;

        } else {
            // Update flags
            atResponseHandlingFlags.is_there_pre_char = false;

            // check if there is a acutual message
            if (atResponseHandlingFlags.word_count>0 && char_in == '\n') {
                // check for OK to terminate processing
                if (strstr(genericResponse.data[atResponseHandlingFlags.word_count-1],"OK")) {
                    genericResponse.num_words = atResponseHandlingFlags.word_count;
                    // call to command CB
                    cb1((enum AT_RESULT_CODES)OK);
                    reset_uart_handling_params();
                }

                // check for ERROR to retry
                if (strstr(genericResponse.data[atResponseHandlingFlags.word_count-1],"ERROR")) {
                    genericResponse.num_words = atResponseHandlingFlags.word_count;
                    // call to command CB
                    cb1((enum AT_RESULT_CODES)ERROR);
                    reset_uart_handling_params();
                }

                // check SMS response
                if ((atResponseHandlingFlags.word_count==2) && strstr(genericResponse.data[0],"+CMT")) {
                    genericResponse.num_words = atResponseHandlingFlags.word_count;
                    // call to command CB
                    cb2();
                    reset_uart_handling_params();
                    reset_smsResponse_reponse();
                }
            }
        
        }

        // user input mode
        if ((atResponseHandlingFlags.word_count==2) && strstr(genericResponse.data[1],"> ")) {
            genericResponse.num_words = atResponseHandlingFlags.word_count;
            cb1((enum AT_RESULT_CODES)OK);
            reset_uart_handling_params();
        }
}

// Function to get RSSI value from +CSQ response
static int get_signal_strength(const char *response) {
    // Check if the response starts with "+CSQ:"
    if (strncmp(response, "+CSQ:", 5) == 0) {
        int rssi = 0;
        // Extract the RSSI value using sscanf
        if (sscanf(response + 6, "%d", &rssi) == 1) {
            return rssi; // Return the extracted RSSI value
        }
    }
    return -1; // Return -1 if parsing failed
}

// Function to get registration status from +CREG response
static int get_registration_status(const char *response) {
    if (strncmp(response, "+CREG:", 6) == 0) {
        int n = 0, status = 0;
        if (sscanf(response + 7, "%d,%d", &n, &status) == 2) {
            return status; // Return the registration status
        }
    }
    return -1; // Failed to parse
}

// Function to get sim status
static int get_sim_status(const char *response) {
    // Check for different possible responses
    if (strstr(response, "+CPIN: READY")) {
        return 1; // SIM is ready
    } else if (strstr(response, "+CPIN: SIM PIN")) {
        return 2; // SIM requires PIN
    } else if (strstr(response, "+CPIN: SIM PUK")) {
        return 3; // SIM requires PUK
    } else if (strstr(response, "+CPIN: NOT INSERTED")) {
        return 4; // SIM not inserted
    } else if (strstr(response, "+CPIN: SIM PIN2")) {
        return 5; // SIM requires PIN2
    } else if (strstr(response, "+CPIN: SIM PUK2")) {
        return 6; // SIM requires PUK2
    }
    return 0; // Unknown or error
}

// Function to send uart commands synchronasly
static enum AT_RESULT_CODES uart_send(const char* cmd){

    reset_uart_handling_params();
    reset_at_command_reponse();

    uint16_t timeout = 2000; // Default timout for general commands

    if(*(cmd)== 0x1A){
        printf("Transmitted data : Ctrl-Z\n");
        timeout = 30000; // 30s for sms response
    } else {
        printf("Transmitted data : %s", cmd);
    }

    uart_puts(gsm_uart_config->uart, cmd);

    uint32_t pre_time = to_ms_since_boot(get_absolute_time());

    // Wait until response is completed
    while(atCommandResponse.error==(enum AT_RESULT_CODES)PROCESSING && to_ms_since_boot(get_absolute_time())-pre_time<timeout){
        sleep_ms(100);
    }

    return atCommandResponse.error;
}

/*************************Public Methods**************************/

// Function to initilize sim800
void sim800_init(UartConfig *uart_config){
    gsm_uart_config = uart_config;

    // Set up our UART with a basic baud rate.
    uart_init(uart_config->uart, uart_config->baudrate);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(uart_config->tx, UART_FUNCSEL_NUM(uart_config->uart, uart_config->tx));
    gpio_set_function(uart_config->rx, UART_FUNCSEL_NUM(uart_config->uart, uart_config->rx));

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(uart_config->uart, false, false);

    // Set our data format
    uart_set_format(uart_config->uart, uart_config->data_bits, uart_config->stop_bit, uart_config->parity);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(uart_config->uart, false);

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = uart_config->uart == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(uart_config->uart, true, false);

}

// Function to deinitialize SIM800
void sim800_deinit(){
    uart_set_irq_enables(gsm_uart_config->uart, false, false);
    int UART_IRQ = gsm_uart_config->uart == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_enabled(UART_IRQ, false);
    irq_remove_handler(UART_IRQ,on_uart_rx);
    uart_deinit(gsm_uart_config->uart);
}

// Function to begin SIM800
enum SIM800_ERROR sim800_begin(){
    printf("SIM800 beginining...\n");
    size_t num_startup_commands = sizeof(startup_at_commands) / sizeof(startup_at_commands[0]);
    enum SIM800_ERROR begin_error= NO_ERROR;

    for (int i = 0; i < num_startup_commands; i++) {
        uint8_t retry_count = 0;
        enum AT_RESULT_CODES error = (enum AT_RESULT_CODES)PROCESSING;
        do {
            error = uart_send(startup_at_commands[i]);
            retry_count++;
        } while((retry_count<RETRY_LIMIT) && (error!= (enum AT_RESULT_CODES)OK));

        if(error!= (enum AT_RESULT_CODES)OK){
            printf("Error occured at sending command : %s\n",startup_at_commands[i] );
            printf("Wait for 10s and reinitializing SIM800.....\n");
            sleep_ms(10000);
            i = -1;
            continue;
        }
    
        // Response is success but we need to check the sim is cofigured properly
        switch(i) {
            case 1 :
                int sim_status = get_sim_status(atCommandResponse.payload.data[1]); 
                if(sim_status==4){
                    // Sim is not inserted
                    printf("SIM not inserted. Deinit the SIM module\n");
                    sim800_deinit();
                    begin_error = (enum SIM800_ERROR) BEGIN_FAILED;
                    i = num_startup_commands;
                }
                break;
            case 2 :
                int signal_strength = get_signal_strength(atCommandResponse.payload.data[1]);
                if(signal_strength==0){
                    // No signal case, need to wait and retry
                    printf("Signal quality is low. Wait for 10S and Retry....\n");
                    sleep_ms(10000);
                    i = 1;
                }
                break;
            case 4 :
                int reg_status  = get_registration_status(atCommandResponse.payload.data[1]);

                if((reg_status!=1) && (reg_status!=5)){
                    // Sim is not registerd yet case, need to wait and retry
                    printf("SIM is not registered. Wait for 10S and Retry....\n");
                    sleep_ms(10000);
                    i = 3;
                }
                break;
            default:
                break;
        }
        sleep_ms(100);
    }

    if (begin_error == (enum SIM800_ERROR)NO_ERROR) {
        printf("SIM800 began successfully\n");
    }

    return begin_error;
}

// Function to send SMS to the recevier
enum SIM800_ERROR sim800_send_sms(const char* msg){
    printf("SIM800 start to send a SMS...\n");

    enum SIM800_ERROR sms_error= NO_ERROR;

    uint8_t retry_count = 0;
    enum AT_RESULT_CODES error = (enum AT_RESULT_CODES)PROCESSING;

    // set sms payload 
    sms_at_commands[1] = msg;

    for (size_t i = 0; i < 3; i++) {
        retry_count=0;
        // send
        do {
            error = uart_send(sms_at_commands[i]);
            retry_count++;
        } while((retry_count<RETRY_LIMIT) && (error!= (enum AT_RESULT_CODES)OK));

        if(error!= (enum AT_RESULT_CODES)OK){
            printf("Error occured at sending sms command : %s\n",sms_at_commands[i] );
            sms_error = (enum SIM800_ERROR) SMS_ERROR;
            break;
        } 
    }
    

    if (sms_error == (enum SIM800_ERROR)NO_ERROR) {
        printf("SIM800 sms sent successfully\n");
    }

    return sms_error;
}

