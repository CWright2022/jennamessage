const char* ssid                    = "crackhome"; //random wifi creds so you don't hack me
const char* password                = "crackhome";
const char* mqtt_server             = "192.168.1.85";
const char* mqtt_user               = "";
const char* mqtt_pass               = "";
const char* my_id                   = "testclient";
const unsigned int mqtt_port        = 1883;
const unsigned int baud             = 19200;

const uint32_t papercheck_milliseconds= 60000L; 

const char* mqtt_listen_topic_text2print     = "printer/text";
// since Version 2
const char* mqtt_listen_topic_textsize       = "printer/text_size";
const char* mqtt_listen_topic_textlineheight = "printer/text_lineheight";
const char* mqtt_listen_topic_textinverse = "printer/text_inverted";
const char* mqtt_listen_topic_textjustify = "printer/text_justify";
const char* mqtt_listen_topic_textbold = "printer/text_bold";
const char* mqtt_listen_topic_textunderline = "printer/text_underline";
const char* mqtt_topic_messages_in_queue = "printer/messages_in_queue";
const char* mqtt_topic_get_messages_in_queue = "printer/get_messages_in_queue";
// since Version 2.1
const char* mqtt_listen_topic_papercheck = "printer/papercheck"; // on this topic, you will recieve every defined 10 seconds the status of the paperload 
//
char mqtt_text_size                  = 'S'; // Letter size - default is S, options are S for Small, M for Medium, L for Large set it via mqtt 
const unsigned int mqtt_row_spacing  = 24; // Spacing between rows - default is 24, values range from minimum of 24 and maximum of 64 set it via mqtt 
