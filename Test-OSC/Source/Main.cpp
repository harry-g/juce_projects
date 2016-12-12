#include "../JuceLibraryCode/JuceHeader.h"

// start size ob blobs sent via OSC - increasing every 10 times
#define START_TEST_SIZE_BYTES 4070

class TestOSC : public JUCEApplication, OSCReceiver, OSCReceiver::Listener<>, Timer {
public:
    TestOSC() {
        // connect OSC sender&receiver
        jassert(sender.connect("127.0.0.1", 9002));
        jassert(connect(9002));
        // listen for OSC messages
        addListener(this);

        // initial size
        data.ensureSize(size);
        // sender timer
        startTimerHz(10);
    }

private:
    MemoryBlock data;
    OSCSender sender;
    int32 size = START_TEST_SIZE_BYTES, counterSender = 0, counterReceiver = 0, counterBlob = 0;


    virtual void timerCallback() override
    {
        // send counter and blob
        sender.send("/test/counter", ++counterSender);
        jassert(sender.send("/test/data", data));

        // after 10 Blobs, increase size by 1
        if (!(counterSender % 10))
            data.ensureSize(++size);
    }

    void oscMessageReceived(const OSCMessage& message) override
    {
        jassert(message.size() == 1);
        String address = message.getAddressPattern().toString();
        // receive counter
        if (message[0].isInt32()) {
            Logger::outputDebugString("Received counter: " + address + " - value: " + String(counterSender));
            // before evaluating next counter, check if corresponding blob was already received
            if (counterBlob != counterReceiver)
                // THIS IS THE BUG I WANT TO SHOW
                Logger::outputDebugString("ERROR: missing blobs are " + String(counterReceiver - counterBlob));
            // store recieved counter
            counterReceiver = message[0].getInt32();
        // receive blob
        } else if (message[0].isBlob()) {
            int32 messageSize = message[0].getBlob().getSize();
            Logger::outputDebugString("Received blob   : " + address + "    - size: " + String(messageSize));
            // count received blobs
            counterBlob++;
        } else
            jassert(false);
    }

    // Inherited via JUCEApplication - not needed here
    virtual const String getApplicationName() override { return String(); }
    virtual const String getApplicationVersion() override { return String(); }
    virtual void initialise(const String & commandLineParameters) override {}
    virtual void shutdown() override {}
};

START_JUCE_APPLICATION(TestOSC)