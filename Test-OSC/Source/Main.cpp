#include "../JuceLibraryCode/JuceHeader.h"
/**
    Copyright (c) 2017 Harry G.
    Permission is granted to use this software under the terms of the GPL v3 (or any later version)
    Details of these licenses can be found at: www.gnu.org/licenses

    This tool tests the max. safe size of blobs via OSC send over UDP.

    Gueranteed in IPv4 Spec is only 576, see https://tools.ietf.org/html/rfc791 (search for "Total Length" or "576")
    IPv4 protocol would allow 65,507 bytes https://en.wikipedia.org/wiki/User_Datagram_Protocol#Packet_structure

    Start without parameters to test on one machine (send & receive).

    Use command line options to test over a network:
     -s send mode - you have to start the receiver first for this to work!
     -r receive mode
    Default is both -s and -r to do both send and receive in parallel on one machine.
    Optionally, use different ports and IP:
     -ip=<address> ip-address or name of the other machine - defaults to 127.0.0.1
     -p=<port> port of the other machine - defaults to 9000
     -fb=<port> port for feedback to sender - defaults to 9001
*/

// start increment size of blobs sent via OSC
#define START_INCREMENT_BYTES 10000
class TestOSCSender : public OSCReceiver, OSCReceiver::Listener<>, Timer {
public:
    TestOSCSender(String ip, int port, int fbPort) {
            printf("Connecting OSC sender...\n");
            jassert(sender.connect(ip, port));

            // sender listens for feedback
            addListener(this);
            jassert(connect(fbPort));

            // sender timer
            startTimerHz(10);
    }

    bool isActive() {
        return active;
    }

    int getLastSize() {
        return sizeToSend;
    }

private:
    MemoryBlock data;
    OSCSender sender;
    bool active = true, success = true;
    int increment = START_INCREMENT_BYTES;
    int sizeToSend = 1;

    void setSuccess() {
        success = true;
    }

    void handleFailed() {
        // go back with current increment
        sizeToSend -= increment * 2;
        // retry with 10-times smaller increment if not yet incremented in steps of 1
        if (increment > 1) {
            increment /= 10;
            printf("Received error - retrying with increment by %d \n", increment);
            sizeToSend++;
        }
    }

    void oscMessageReceived(const OSCMessage& message) override
    {
        String address = message.getAddressPattern().toString();
        if (address.contains("success")) {
            setSuccess();
        }
        if (address.contains("failed")) {
            handleFailed();
        }
        if (address.contains("stop")) {
            printf("Received stop.\n");
            stopTimer();
            sizeToSend -= increment * 2;
            active = false;
        }
    }

    virtual void timerCallback() override
    {
        if (success) {
            // increase size
            data.reset();
            data.ensureSize(sizeToSend);
            // send counter and blob
            printf("Sending blob with size  : %d \n", data.getSize());
            jassert(sender.send("/test/data", data));
            sender.send("/test/size", sizeToSend);

            sizeToSend += increment;
        }
    }
};

class TestOSCReceiver : public OSCReceiver, OSCReceiver::Listener<> {
public:
    TestOSCReceiver(String ip, int port, int fbPort) {
        // listen for OSC messages
        addListener(this);

        printf("Connecting OSC receiver...\n");
        jassert(connect(port));
        jassert(fbSender.connect(ip, fbPort));
    }

    bool isActive() {
        return active;
    }

    int getLastSize() {
        return sizeReceived;
    }

private:
    OSCSender fbSender;
    int32  sizeExpected = 0, sizeReceived = 0;
    bool active = true;

    void oscMessageReceived(const OSCMessage& message) override
    {
        jassert(message.size() == 1);
        // receive counter
        if (message[0].isInt32()) {
            int32 lastIncrement = message[0].getInt32() - sizeExpected;
            sizeExpected = message[0].getInt32();
            printf("Received expected size  : %d ... ", sizeExpected);
            // before evaluating next counter, check if corresponding blob was already received
            if (sizeExpected != sizeReceived) {
                // if not yet incremented in steps of 1
                if (lastIncrement > 1) {
                    // give error feedback for retry
                    printf("Detected missing blob! Sending Retry.\n");
                    fbSender.send("/test/failed");
                }
                // incremented in steps of 1? That's it!
                else {
                    printf("Detected missing blob! Sending Stop.\n");
                    fbSender.send("/test/stop");
                    active = false;
                }
            }
            else {
                // successfully received expected size, request next one
                printf("OK!\n");
                fbSender.send("/test/success");
            }
            // receive blob
        }
        else if (message[0].isBlob()) {
            sizeReceived = message[0].getBlob().getSize();
            printf("Received blob with size : %d\n", sizeReceived);
        }
        else
            jassert(false);
    }
};

class TestOSC : public JUCEApplication, Timer {
public:
    TestOSC() {
        printf("TestOSC\n=======\nTest tool for testing max. OSC blob size in a network.\n\n");

        // check command line options
        StringArray params = getCommandLineParameterArray();
        if (params.size() == 0) {
            // show comandline help, but run anyway with defaults
            printf("Comandline parameters are:\n");
            printf(" -s - send mode - you have to start the receiver first for this to work!\n");
            printf(" -r - receive mode\n");
            printf("Default is both - s and -r to do both send and receive in parallel on one machine.\n");
            printf("Optionally, use different ports and IP:\n");
            printf(" -ip<address> ip-address or name of the other machine - defaults to 127.0.0.1\n");
            printf(" -p<port> port of the other machine - defaults to 9000\n");
            printf(" -fb<port> port for feedback to sender - defaults to 9001\n");
            printf("Running wit default parameters now...\n\n");
        }
        for (auto param : params) {
            if (param.startsWithIgnoreCase("-ip")) {
                ip = param.substring(3);
                printf("Using IP %s\n", ip);
            }
            if (param.startsWithIgnoreCase("-p")) {
                port = param.substring(2).getIntValue();
                printf("using port %d\n", port);
            }
            if (param.startsWithIgnoreCase("-fb")) {
                fbPort = param.substring(3).getIntValue();
                printf("using feedback port %d\n", fbPort);
            }
            if (param.startsWithIgnoreCase("-s")) {
                printf("Send mode selected\n");
                send = true;
            }
            if (param.startsWithIgnoreCase("-r")) {
                printf("Receive mode selected\n");
                receive = true;
            }
        }

        // no option given? send & receive
        if (!send && !receive) {
            printf("Using default option send & receive on one machine.\n\n");
            send = true;
            receive = true;
        }

        if (send)
            sender = new TestOSCSender(ip, port, fbPort);
        if (receive)
            receiver = new TestOSCReceiver(ip, port, fbPort);
        printf("\n");

        startTimerHz(10);
    }

    ~TestOSC() {
        if (send)
            delete sender;
        if (receive)
            delete receiver;
    }

private:
    TestOSCSender* sender;
    TestOSCReceiver* receiver;
    bool send = false, receive = false;
    String ip = "127.0.0.1";
    int port = 9000, fbPort = 9001;

    // Inherited via JUCEApplication - not needed here
    virtual const String getApplicationName() override { return String(); }
    virtual const String getApplicationVersion() override { return String(); }
    virtual void initialise(const String & ) override {}
    virtual void shutdown() override {}

    // Inherited via Timer
    virtual void timerCallback() override
    {
        if (send && !sender->isActive() || receive && !receiver->isActive()) {
            // show last successful size
            int size;
            // prefered is received size
            if (receive)
                size = receiver->getLastSize();
            // only as info on sender side if running standalone
            else
                size = sender->getLastSize();
            printf("\nRESULT: Max. safe OSC blob size in this setup is %d bytes\n", size);
            printf("\nPress Enter to exit...\n");
            getchar();
            stopTimer();
            quit();
        }
    }
};

START_JUCE_APPLICATION(TestOSC)