// Full-screen gradient fade overlay
#include <channel/Fade.h>
#include <channel/ColorWhite.h>
#include <channel/DrawUtil.h>
#include <channel/System.h>

Fade::Fade(nw4r::ut::Color color)
    : mBaseColor(color), mState(NULL), mStatePhase(0), mFrames(0), mAlpha(0.0f), mStep(0.0f) {
    mQuad[0].x = 0.0f;
    mQuad[0].y = 0.0f;
    mQuad[0].z = 0.0f;
    mQuad[1].x = 0.0f;
    mQuad[1].y = GetScreenHeight();
    mQuad[1].z = 0.0f;
    mQuad[2].x = GetScreenWidth();
    mQuad[2].y = GetScreenHeight();
    mQuad[2].z = 0.0f;
    mQuad[3].x = GetScreenWidth();
    mQuad[3].y = 0.0f;
    mQuad[3].z = 0.0f;

    for (int i = 0; i < 4; i++) {
        mColors[i].r = color.r;
        mColors[i].g = color.g;
        mColors[i].b = color.b;
        mColors[i].a = 0;
    }

    ChangeState(&Fade::StateIdle);
}

Fade::~Fade() {}

void Fade::Draw() {
    if (mColors[0].a != 0) {
        SetDefaultGXState();
        SetOrthoProjection();
        DrawGradientQuad(mQuad, (const GXColor*)mColors);
    }
}

void Fade::Calc() {
    mQuad[1].y = GetScreenHeight();
    mQuad[2].x = GetScreenWidth();
    mQuad[2].y = GetScreenHeight();
    mQuad[3].x = GetScreenWidth();

    if (mState) {
        (this->*mState)();
    }
}

void Fade::FadeIn(s32 frames) {
    mFrames = frames;
    ChangeState(&Fade::StateFadeIn);
}

void Fade::FadeOut(s32 frames) {
    mFrames = frames;
    ChangeState(&Fade::StateFadeOut);
}

static inline void SetColorsAlpha(Color* colors, u8 alpha) {
    colors[0].a = alpha;
    colors[1].a = alpha;
    colors[2].a = alpha;
    colors[3].a = alpha;
}

BOOL Fade::StateIdle() {
    switch (mStatePhase) {
    case 0:
        mStatePhase++;
        mFading = 0;
        break;
    case -1:
        break;
    }

    return TRUE;
}

BOOL Fade::StateFadeIn() {
    switch (mStatePhase) {
    case 0:
        mStatePhase++;
        mFading = 1;
        mAlpha = 1.0f;
        mStep = 1.0f / mFrames;
        SetColorsAlpha(mColors, mBaseColor.a * mAlpha);
        break;
    case -1:
        break;
    default:
        mAlpha -= mStep;
        if (mAlpha <= 0.0f) {
            mAlpha = 0.0f;
            ChangeState(&Fade::StateIdle);
        }
        SetColorsAlpha(mColors, mBaseColor.a * mAlpha);
        break;
    }

    return TRUE;
}

BOOL Fade::StateFadeOut() {
    switch (mStatePhase) {
    case 0:
        mStatePhase++;
        mFading = 2;
        mStep = 1.0f / mFrames;
        mAlpha = 0.0f;
        SetColorsAlpha(mColors, mBaseColor.a * mAlpha);
        break;
    case -1:
        break;
    default:
        mAlpha += mStep;
        if (mAlpha >= 1.0f) {
            mAlpha = 1.0f;
            ChangeState(&Fade::StateIdle);
        }
        SetColorsAlpha(mColors, mBaseColor.a * mAlpha);
        break;
    }

    return TRUE;
}

void Fade::SetOpaque() {
    mAlpha = 1.0f;
    SetColorsAlpha(mColors, mBaseColor.a);
}
