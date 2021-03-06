#pragma once

#include "public.sdk/source/vst2.x/audioeffectx.h"

#include "Defines.h"
#include "vstgui.h"

class CSmartelectronixDisplay : public AudioEffectX {
public:
    /*
trigger type : free / rising / falling / internal / external   (this order)
channel : left / right
all else : on / off
*/

    // VST parameters
    enum {
        kTriggerSpeed, // internal trigger speed, knob
        kTriggerType, // trigger type, selection
        kTriggerLevel, // trigger level, slider
        kTriggerLimit, // retrigger threshold, knob
        kTimeWindow, // X-range, knob
        kAmpWindow, // Y-range, knob
        kSyncDraw, // sync redraw, on/off
        kChannel, // channel selection, left/right
        kFreeze, // freeze display, on/off
        kDCKill, // kill DC, on/off
        kNumParams
    };

    // VST elements
    enum {
        kNumPrograms = 0,
        kNumInputChannels = 2,
        kNumOutputChannels = 2, // VST doesn't like 0 output channels ;-)
    };

    // trigger types
    enum {
        kTriggerFree = 0,
        kTriggerRising,
        kTriggerFalling,
        kTriggerInternal,
        kNumTriggerTypes
    };

    CSmartelectronixDisplay(audioMasterCallback audioMaster);
    ~CSmartelectronixDisplay();

    virtual void process(float** inputs, float** outputs, VstInt32 sampleFrames);
    virtual void processReplacing(float** inputs, float** outputs,
        VstInt32 sampleFrames);

    virtual void setParameter(VstInt32 index, float value);
    virtual float getParameter(VstInt32 index);
    virtual void getParameterLabel(VstInt32 index, char* label);
    virtual void getParameterDisplay(VstInt32 index, char* text);
    virtual void getParameterName(VstInt32 index, char* text);

    void getDisplay(VstInt32 index, char* text);

    virtual VstInt32 canDo(char* text);
    virtual bool getEffectName(char* name);
    virtual bool getVendorString(char* text);
    virtual bool getProductString(char* text);

    virtual void suspend();
    virtual void resume();

    const std::vector<CPoint>& getPeaks() const { return peaks; }
    const std::vector<CPoint>& getCopy() const { return copy; }

protected:
    std::vector<CPoint> peaks;
    std::vector<CPoint> copy;

    // the actual algo :-)
    void processSub(float** inputs, long sampleFrames);

    // index into the peak-array
    unsigned long index;

    // counter which is used to set the amount of samples / pixel
    double counter;

    // max/min peak in this block
    float max, min, maxR, minR;

    // the last peak we encountered was a maximum!
    bool lastIsMax;

    // the previous sample (for edge-triggers)
    float previousSample;

    // the internal trigger oscillator
    double triggerPhase;

    // stupid VST parameter save
    float SAVE[kNumParams];

    // trigger limiter!
    long triggerLimitPhase;

    // dc killer
    double dcKill, dcFilterTemp;
};
