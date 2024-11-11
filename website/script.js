const brokerAddress = "wss://broker.hivemq.com:8884/mqtt"
const client = mqtt.connect(brokerAddress);
    client.on('connect', () => {
        console.log('Connected to MQTT broker:', brokerAddress);
    });

//get values from HTML form
function getValues(event) {
    // Prevent form submission from reloading the page
    event.preventDefault();
    //use object for storage
    messageProperties={
        message: null,
        text_size: null,
        justify: null,
        bold: null,
        underline: null,
        inverted: null
    };
    // Get relevant values
    messageProperties.message = document.getElementById("message").value;
    messageProperties.text_size = document.getElementById("text_size").value;
    messageProperties.justify = document.getElementById("justify").value;
    messageProperties.bold = document.getElementById("bold").checked ? "1" : "0";
    messageProperties.underline = document.getElementById("underline").checked ? "1" : "0";
    messageProperties.inverted = document.getElementById("inverted").checked ? "1" : "0";
    console.log(messageProperties);
    sendMessage(messageProperties);
}

//send to relevant mqtt topics
function sendMessage(messageProperties){
    client.publish("printer/text_inverted", messageProperties.inverted);
    client.publish("printer/text_underline", messageProperties.underline);
    client.publish("printer/text_bold", messageProperties.bold);
    client.publish("printer/text_justify", messageProperties.justify);
    client.publish("printer/text_size", messageProperties.text_size);
    client.publish("printer/text", messageProperties.message);
}