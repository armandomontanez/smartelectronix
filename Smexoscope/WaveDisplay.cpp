// CustomVU.cpp: implementation of the CCustomVU class.
//
//////////////////////////////////////////////////////////////////////

#include "WaveDisplay.h"
#include "math.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------
// CVuMeter
//------------------------------------------------------------------------
CWaveDisplay::CWaveDisplay(const CRect& size, CSmartelectronixDisplay* effect, CBitmap* back, CBitmap* heads, CBitmap* readout)
    : CControl(size, 0, 0)
    , effect(effect)
    , back(back)
    , heads(heads)
    , readout(readout)
    , counter(0)
{
    value = oldValue = 0.f;

    srand((unsigned)time(NULL));
    display = rand() % 4;

    if (heads)
        heads->remember();
    if (back)
        back->remember();
    if (readout)
        readout->remember();

    where.x = -1;
    OSDC = 0;
    timer = new CVSTGUITimer(this, 1000/30, true);
}

CMessageResult CWaveDisplay::notify (CBaseObject* sender, IdStringPtr message)
{
    if (message == CVSTGUITimer::kMsgTimer)
    {
        invalid();

        return kMessageNotified;
    }
    return kMessageUnknown;
}

//------------------------------------------------------------------------
CWaveDisplay::~CWaveDisplay()
{
    if (timer)
        timer->forget();
    if (heads)
        heads->forget();
    if (back)
        back->forget();
    if (readout)
        readout->forget();

    if (OSDC != 0)
        OSDC->forget();
}

CMouseEventResult CWaveDisplay::onMouseDown(CPoint& where, const CButtonState& buttons)
{
    if (buttons.isLeftButton() && getViewSize().pointInside(where))
    {
        this->where = where;
        return CMouseEventResult::kMouseEventHandled;
    }

    where.x = -1;

    return CMouseEventResult::kMouseEventNotHandled;
}

CMouseEventResult CWaveDisplay::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
    if (buttons.isLeftButton() && getViewSize().pointInside(where))
    {
        this->where = where;
        return CMouseEventResult::kMouseEventHandled;
    }

    where.x = -1;

    return CMouseEventResult::kMouseEventNotHandled;
}

CMouseEventResult CWaveDisplay::onMouseUp(CPoint& where, const CButtonState& buttons)
{
    where.x = -1;

    return CMouseEventResult::kMouseEventHandled;
}

inline float cf_lin2db(float lin)
{
    if (lin < 9e-51)
        return -1000.f; /* prevent invalid operation */
    return 20.f * log10f(lin);
}

//------------------------------------------------------------------------
void CWaveDisplay::draw(CDrawContext* pContext)
{
    if (OSDC == 0)
        OSDC = COffscreenContext::create(getFrame(), size.getWidth(), size.getHeight());

    // background, stupid hack, for some reason the rect is offset one pixel??
    CRect R(0, 0, size.getWidth(), size.getHeight());

    back->draw(OSDC, R, CPoint(size.left, size.top));

    char text[256];
    sprintf(text, "counter=%ld", counter++);
    OSDC->drawString(text, CRect(0, 0, 100, 100), kLeftText);


    R(615 - size.left, 240 - size.top, 615 + heads->getWidth() - size.left, 240 + heads->getHeight() / 4 - size.top);
    heads->draw(OSDC, R, CPoint(0, (display * heads->getHeight()) / 4));

    OSDC->setDrawMode(CDrawMode(kAntiAliasing));

    // trig-line
    long triggerType = (long)(effect->getParameter(CSmartelectronixDisplay::kTriggerType) * CSmartelectronixDisplay::kNumTriggerTypes + 0.0001);

    if (triggerType == CSmartelectronixDisplay::kTriggerRising || triggerType == CSmartelectronixDisplay::kTriggerFalling) {
        long y = 1 + (long)((1.f - effect->getParameter(CSmartelectronixDisplay::kTriggerLevel)) * (size.getHeight() - 2));

        CColor grey = { 229, 229, 229 };
        OSDC->setFrameColor(grey);
        OSDC->drawLine(CPoint(0, y), CPoint(size.getWidth() - 1, y));
    }

    // zero-line
    CColor orange = { 179, 111, 56 };
    OSDC->setFrameColor(orange);
    OSDC->drawLine(CPoint(0, size.getHeight() / 2 - 1), CPoint(size.getWidth() - 1, size.getHeight() / 2 - 1));

    // waveform
    const std::vector<CPoint>& points = (effect->getParameter(CSmartelectronixDisplay::kSyncDraw) > 0.5f) ? effect->getCopy() : effect->getPeaks();
    double counterSpeedInverse = pow(10.f, effect->getParameter(CSmartelectronixDisplay::kTimeWindow) * 5.f - 1.5);

    if (counterSpeedInverse < 1.0) //draw interpolated lines!
    {
        CColor blue = { 64, 148, 172 };
        OSDC->setFrameColor(blue);

        double phase = counterSpeedInverse;
        double dphase = counterSpeedInverse;

        long prevxi = points[0].x;
        long prevyi = points[0].y;

        for (long i = 1; i < size.getWidth() - 1; i++) {
            long index = (long)phase;
            double alpha = phase - (double)index;

            long xi = i;
            long yi = (long)((1.0 - alpha) * points[index * 2].y + alpha * points[(index + 1) * 2].y);

            OSDC->drawLine(CPoint(prevxi, prevyi), CPoint(xi, yi));
            prevxi = xi;
            prevyi = yi;

            phase += dphase;
        }
    } else {
        CColor grey = { 118, 118, 118 };
        OSDC->setFrameColor(grey);

        for (unsigned int i=0; i<points.size()-1; i++)
            OSDC->drawLine(points[i], points[i+1]);
    }

    //TODO clean this mess up...
    if (where.x != -1) {
        CPoint whereOffset = where;
        whereOffset.offset(-size.left, -size.top);
        CDrawMode oldMode = OSDC->getDrawMode();

        // OSDC->setDrawMode(kXorMode);

        OSDC->drawLine(CPoint(0, whereOffset.y), CPoint(size.getWidth() - 1, whereOffset.y));
        OSDC->drawLine(CPoint(whereOffset.x, 0), CPoint(whereOffset.x, size.getHeight() - 1));

        float gain = powf(10.f, effect->getParameter(CSmartelectronixDisplay::kAmpWindow) * 6.f - 3.f);
        float y = (-2.f * ((float)whereOffset.y + 1.f) / (float)OSC_HEIGHT + 1.f) / gain;
        float x = (float)whereOffset.x * (float)counterSpeedInverse;
        char text[256];

        long lineSize = 10;
        //long border = 2;

        //CColor color = {179,111,56,0};
        CColor color = { 169, 101, 46, 0 };

        OSDC->setFontColor(color);

        OSDC->setFont(kNormalFontSmaller);

        readout->draw(OSDC, CRect(508, 8, 508 + readout->getWidth(), 8 + readout->getHeight()), CPoint(0, 0));

        CRect textRect(510, 10, 650, 10 + lineSize);

        sprintf(text, "y = %.5f", y);
        OSDC->drawString(text, textRect, kLeftText);
        textRect.offset(0, lineSize);

        sprintf(text, "y = %.5f dB", cf_lin2db(fabsf(y)));
        OSDC->drawString(text, textRect, kLeftText);
        textRect.offset(0, lineSize * 2);

        sprintf(text, "x = %.2f samples", x);
        OSDC->drawString(text, textRect, kLeftText);
        textRect.offset(0, lineSize);

        sprintf(text, "x = %.5f seconds", x / effect->getSampleRate());
        OSDC->drawString(text, textRect, kLeftText);
        textRect.offset(0, lineSize);

        sprintf(text, "x = %.5f ms", 1000.f * x / effect->getSampleRate());
        OSDC->drawString(text, textRect, kLeftText);
        textRect.offset(0, lineSize);

        if (x == 0)
            sprintf(text, "x = infinite Hz");
        else
            sprintf(text, "x = %.3f Hz", effect->getSampleRate() / x);

        OSDC->drawString(text, textRect, kLeftText);

        OSDC->setDrawMode(oldMode);
    }

    OSDC->copyFrom(pContext, size, CPoint(0, 0));
}
