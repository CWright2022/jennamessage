var client;
var brokerAddress = "wss://vpn.caydenwright.com:8000/mqtt"
errorMessage=document.getElementById("errorMessage");
button = document.getElementById("submit_button");
messagesInQueueLabel = document.getElementById("messagesInQueue");


function connect(event){
    event.preventDefault();
    password = document.getElementById("password_field").value
    const options = {
        clean: true,
        connectTimeout: 4000,
        clientId: "webclient_cwlFA3EdRCYYT59k",
        username: "webclient_cwlFA3EdRCYYT59k",
        password: password,
    }
    client = mqtt.connect(brokerAddress, options);

    client.on("message", function(topic, message){
        if(topic == "printer/messages_in_queue"){
            console.log("messages in queue: ".concat(message.toString()));
            messagesInQueueLabel.textContent = "Messages in queue: ".concat(message.toString());
            if(parseInt(message.toString())>=10){
                console.log("too many messages");
                errorMessage.textContent = "There are too many messages (>10) in the queue currently. Try again later.";
                button.disabled=true;
            }
            else{
                button.disabled=false;
                errorMessage.textContent = "";
            }
        }
    });

    client.on("connect", () => {
        console.log("connected");
        client.subscribe("printer/messages_in_queue", function(err){
            if(err){
                console.log(err);
                errorMessage.textContent = "Error subscribing to relevant topics. Refresh the page or try again later.";
                client.end();
            }
        });
        console.log("Connected to MQTT broker:", brokerAddress);
        button.disabled=false;
        errorMessage.textContent = "";
        const modal = document.getElementById("password_modal");
        modal.classList.add("hidden"); // Hide the modal
        client.publish("printer/get_messages_in_queue", "get");
    });

    client.stream.on("error", (err) => {
        console.log("Error connecting to broker:\n", err);
        errorMessage.textContent = "Error connecting to MQTT broker. Refresh the page or try again later.";
    });

    client.on("error", (err) => {
        console.error("Connection error:", err.message);
      
        if (err.message.includes("Not authorized")) {
          console.error("Authentication failed: Wrong username or password.");
          document.getElementById("passwordStatusMessage").classList.add("visible");
        }
      
        client.end(); // Optional: Cleanly end the connection
      });
}

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
    client.publish("printer/get_messages_in_queue", "get");
    document.getElementById("messageSent").textContent = "Message sent successfully!";
    document.getElementById("messageSent").classList.add("visible");
    document.getElementById("message").value="";
    setTimeout(function(){
        document.getElementById("messageSent").textContent = "";
        messageSent.classList.remove("visible"); // Hide the message
    }, 3000);
    setTimeout(function(){
        document.getElementById("errorMessage").textContent = "";
    }, 5000);
}