// d_weather_base.cpp: base of the forecast panels
#include <channel/WeatherBase.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/System.h>

#include <revolution/GX.h>

extern TPLPalette* gCommonTpl;
extern f32 gUnkSceneFloat;

WeatherBase::WeatherBase(const Vec2& pos, const Vec2& size, s32 type)
    : mLayout(NULL), mPos(pos), mSize(size), mRect(0.0f, 0.0f, GetScreenWidth(), 456.0f), mScale(1.0f), mType(type),
      unk90(0), mAlpha(255), unk98(-1), mCulled(FALSE), mVisible(TRUE) {
    f32 x = mPos.x - 0.5f * GetScreenWidth();
    f32 y = mPos.y - 228.0f;
    mRect.MoveTo(x, y);
}

void WeatherBase::SetPosition(const Vec2& pos, const f32& scale, const u8& visible, BOOL checkHover) {
    mPos = pos;
    mScale = scale;
    mVisible = visible;
    mRect.MoveTo(pos.x, -pos.y);

    if (!mVisible || mRect.top >= 456.0f || mRect.bottom <= 0.0f || mRect.left >= GetScreenWidth() ||
        mRect.right <= 0.0f) {
        mCulled = TRUE;
    } else {
        mCulled = FALSE;
    }
}

void WeatherBase::DrawIcon(TextBox* box, u32 id, s32 alpha) {
    f32 halfWidth = box->mScaleX * (0.5f * GetTexWidth(gCommonTpl, id));
    f32 halfHeight = 0.5f * GetTexHeight(gCommonTpl, id);
    Vec pos;
    Vec shadowPos;

    pos.x = box->mX + mPos.x - halfWidth;
    pos.y = box->mY - mPos.y - box->mScaleX * halfHeight;
    pos.z = 0.0f;
    shadowPos.x = 2.0f + pos.x;
    shadowPos.y = 2.0f + pos.y;
    shadowPos.z = 0.0f;

    box->mShadowColor.a = alpha;
    box->mColor.a = alpha;
    GXSetTevColor(GX_TEVREG0, box->mShadowColor);
    DrawTextureAt(gCommonTpl, id, box->mScaleX, box->mScaleX, &shadowPos);
    GXSetTevColor(GX_TEVREG0, box->mColor);
    DrawTextureAt(gCommonTpl, id, box->mScaleX, box->mScaleX, &pos);
}

void WeatherBase::DrawIconLarge(TextBox* box, u32 id, s32 alpha) {
    f32 halfWidth = box->mScaleX * (0.5f * GetTexWidth(gCommonTpl, id));
    f32 halfHeight = 0.5f * GetTexHeight(gCommonTpl, id);
    Vec pos;
    Vec shadowPos;

    pos.x = box->mX + mPos.x - halfWidth;
    pos.y = box->mY - mPos.y - box->mScaleX * halfHeight;
    pos.z = 0.0f;
    shadowPos.x = 3.0f + pos.x;
    shadowPos.y = 3.0f + pos.y;
    shadowPos.z = 0.0f;

    box->mShadowColor.a = alpha;
    box->mColor.a = alpha;
    GXSetTevColor(GX_TEVREG0, box->mShadowColor);
    DrawTextureAt(gCommonTpl, id, box->mScaleX, box->mScaleX, &shadowPos);
    GXSetTevColor(GX_TEVREG0, box->mColor);
    DrawTextureAt(gCommonTpl, id, box->mScaleX, box->mScaleX, &pos);
}

void WeatherBase::DrawText(TextBox* box, const wchar_t* text, s32 alpha) {
    Vec2 pos;
    f32 scaleX;
    f32 scaleY;

    pos.y = box->mY - mPos.y;
    pos.x = box->mX + mPos.x;
    box->mShadowColor.a = alpha;
    box->mColor.a = alpha;

    mWriter.SetScale(box->mScaleX, box->mScaleY);
    mWriter.SetCharSpace(gUnkSceneFloat * box->mScaleX);
    mWriter.SetLineSpace(0.0f);

    f32 width = mWriter.CalcStringWidth(text);
    if (width > box->mWidth) {
        scaleX = box->mScaleX * (box->mWidth / width);
        mWriter.SetCharSpace(scaleX * gUnkSceneFloat);
    } else {
        scaleX = box->mScaleX;
    }

    f32 height = mWriter.CalcStringHeight(text);
    if (height > box->mHeight) {
        scaleY = box->mScaleY * (box->mHeight / height);
        mWriter.SetLineSpace(-4.0f);
    } else {
        scaleY = box->mScaleY;
    }

    scaleX = scaleX < scaleY ? scaleX : scaleY;

    mWriter.SetScale(scaleX);
    mWriter.SetTextColor(box->mShadowColor);
    mWriter.SetCursor(1.0f + pos.x, 1.0f + pos.y);
    mWriter.Print(text);
    mWriter.SetTextColor(box->mColor);
    mWriter.SetCursor(pos.x, pos.y);
    mWriter.Print(text);
}
