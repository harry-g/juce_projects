#include "../JuceLibraryCode/JuceHeader.h"

// start size of blobs sent via OSC
#define START_TEST_SIZE_BYTES 4000

/**
    Tests the max. safe size of blobs via OSC.

    Start without parameters to test on this machine (send & receive).

    Use command line options to test over a network:
    -ip=<address> ip-address (not name!) of the other machine - defaults to 127.0.0.1
    -port=<port> port of the other machine - defaults to 9000
    -bs=<size> the blob size to start the test with - defaults to 4000, which works in most cases. Gueranteed in IPv4 Spec is only 576!
    -s send only - you have to start the receiver first for this to work!
    -r receive only
    Default is both -s and -r to do both send and receive in parallel on one machine.
*/
class TestOSC : public JUCEApplication, OSCReceiver, OSCReceiver::Listener<>, Timer {
public:
    TestOSC() {
        // check command line options
        StringArray params = getCommandLineParameterArray();
        for (auto param : params) {
            if (param.startsWithIgnoreCase("-ip="))
                ip = param.substring(4);
            if (param.startsWithIgnoreCase("-port="))
                port = param.substring(6).getIntValue();
            if (param.startsWithIgnoreCase("-s"))
                send = true;
            if (param.startsWithIgnoreCase("-r"))
                receive = true;
            if (param.startsWithIgnoreCase("-bs="))
                size = param.substring(4).getIntValue();
        }

        // no option given? send & receive
        if (!send && !receive) {
            printf("Using default option send & receive on one machine.\n");
            send = true;
            receive = true;
        }

        if (receive) {
            printf("Connecting OSC receiver...\n");
            jassert(connect(port));
            // listen for OSC messages
            addListener(this);
        }


        if (send) {
            printf("Connecting OSC sender...\n");
            jassert(sender.connect(ip, port));
            // initial size
            data.ensureSize(size);
            // sender timer
            startTimerHz(10);
        }
    }

    ~TestOSC() {
        stopTimer();
    }
    
private:
    MemoryBlock data;
    OSCSender sender;
    int32 size = START_TEST_SIZE_BYTES, counterSender = 0, counterReceiver = 0, counterBlob = 0, blobSize = 0;
    bool send = false, receive = false, stop = false;
    String ip = "127.0.0.1";
    int port = 9000;

    virtual void timerCallback() override
    {
        // send counter and blob
        sender.send("/test/counter", ++counterSender);
        printf("Sending blob    : %d\n", counterSender);
        jassert(sender.send("/test/data", data));
        // increase size by 1
        data.ensureSize(++size);
    }

    void oscMessageReceived(const OSCMessage& message) override
    {
        jassert(message.size() == 1);
        // receive counter
        if (message[0].isInt32()) {
            printf("Received counter: %s - value: %d\n", message.getAddressPattern().toString().getCharPointer(), counterSender);
            // before evaluating next counter, check if corresponding blob was already received
            if (counterBlob != counterReceiver) {
                stopTimer();
                // missing blob - show last successful size
                printf("Detected missing blob!\n");
                printf("Max. blob size in this setup is %d bytes\n", blobSize);
                printf("Press any key to exit...\n");
                getchar();
                quit();
            }
            // store recieved counter
            counterReceiver = message[0].getInt32();
        // receive blob
        } else if (message[0].isBlob()) {
            blobSize = message[0].getBlob().getSize();
            printf("Received blob   : %s    - size : %d\n", message.getAddressPattern().toString().getCharPointer(), blobSize);
            // count received blobs
            counterBlob++;
        } else
            jassert(false);
    }

    // Inherited via JUCEApplication - not needed here
    virtual const String getApplicationName() override { return String(); }
    virtual const String getApplicationVersion() override { return String(); }
    virtual void initialise(const String & ) override {}
    virtual void shutdown() override {}
};

START_JUCE_APPLICATION(TestOSC)