// d_weather_address.cpp: area/city picker
#include <channel/WeatherViews.h>
#include <channel/ColorWhite.h>
#include <channel/DrawUtil.h>
#include <channel/ForecastData.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/System.h>
#include <channel/WeatherScene.h>

#include <wstring.h>

extern nw4r::ut::Font* gSysFont;
extern TPLPalette* gCommonTpl;
extern Color gHighlightColor;
extern u8 gViewsCreated;
extern s32 gCursorState[4];
extern f32 gDragScroll[];

f32 SmoothApproach(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep);
void UpdateCurrentCity();

static f32 sOtherEntryX = 0.0f;
static f32 sOtherEntryY = 0.0f;

static Color sListColor(255, 255, 255, 64);
static Color sTextColor(255, 255, 255, 255);

#define CHANGE_STATE(state, arg)                                                                             \
    {                                                                                                        \
        StateFunc newState = state;                                                                          \
        if (mState) {                                                                                        \
            mPhase = -1;                                                                                     \
            (this->*mState)(arg);                                                                            \
        }                                                                                                    \
        mPhase = 0;                                                                                          \
        mState = newState;                                                                                   \
        if (mState) {                                                                                        \
            (this->*mState)(arg);                                                                            \
        }                                                                                                    \
    }

#define CHANGE_SCROLL(state)                                                                                 \
    {                                                                                                        \
        ScrollFunc newState = state;                                                                         \
        if (mScrollState) {                                                                                  \
            mScrollPhase = -1;                                                                               \
            (this->*mScrollState)();                                                                         \
        }                                                                                                    \
        mScrollState = newState;                                                                             \
        mScrollPhase = 0;                                                                                    \
        if (mScrollState) {                                                                                  \
            (this->*mScrollState)();                                                                         \
        }                                                                                                    \
    }

#define FIND_AREA(result)                                                                                    \
    {                                                                                                        \
        AddressEntry* entry = mEntries;                                                                      \
        result = -1;                                                                                         \
        for (s32 i = 0; i < mNumPlaces; i++, entry++) {                                                      \
            if (mAreaId == (*entry->mPlace->mId & 0xFFFF0000)) {                                             \
                result = i;                                                                                  \
                break;                                                                                       \
            }                                                                                                \
        }                                                                                                    \
    }

WeatherAddress::WeatherAddress(void* arc)
    : mEntries(NULL), mSelected(NULL), mListHead(NULL), mActiveLayout(NULL), mBaseLayout(NULL), mListLayout(NULL),
      mConfirmLayout(NULL), mState(NULL), mDrawFunc(NULL), mScrollState(NULL) {
    mListX = 0.5f * GetScreenWidth();
    mScroll = 0.0f;
    mUpArrowPos.x = 0.0f;
    mUpArrowPos.y = 0.0f;
    mUpArrowPos.z = 0.0f;
    mDownArrowPos.x = 0.0f;
    mDownArrowPos.y = 0.0f;
    mDownArrowPos.z = 0.0f;
    mListTop = 100.0f;
    mRowHeight = 60.0f;
    mClipTop = 100.0f;
    mClipBottom = 393.0f;
    mScrollTarget = 0.0f;
    mScrollMax = 0.0f;
    mScrollMin = 0.0f;
    mScrollSpeed = 0.0f;
    mNumPlaces = 0;
    mNumEntries = 0;
    mNumPages = 0;
    mTopIndex = 0;
    mPhase = 0;
    mScrollPhase = 0;
    mTimer = 0;
    mUp = FALSE;
    mDown = FALSE;
    mDragging = FALSE;
    mYes = FALSE;
    mNo = FALSE;
    mCanGoBack = TRUE;
    mShowArrows = FALSE;

    s32 numPlaces = gForecastData->mHeader->mNumPlaces;
    mWriter.SetFont(*gSysFont);
    mWriter.SetCharSpace(0.0f);
    mWriter.SetScale(1.0f);

    f32 maxWidth = GetScreenWidth() - (gWidescreen ? 36 : 28) - (gWidescreen ? 36 : 28);
    mNumPlaces = numPlaces;
    mEntries = new AddressEntry[numPlaces];

    AddressEntry* entry = mEntries;
    CityInfo* place = gForecastData->mPlaces;
    for (s32 i = 0; i < numPlaces; i++, place++, entry++) {
        entry->mPlace = place;

        s32 len = 0;
        if (place->mRegion != NULL) {
            len = wcslen(place->mRegion);
        } else if (place->mCountry != NULL) {
            len = wcslen(place->mCountry);
        }

        len += wcslen(place->mName);
        if (len != 0) {
            len++;
        }

        entry->mName = new wchar_t[len];
        wcscpy(entry->mName, place->mName);

        f32 width = mWriter.CalcStringWidth(entry->mName);
        if (width > maxWidth) {
            entry->mNameScaleX = maxWidth / width;
        } else {
            entry->mNameScaleX = 1.0f;
        }

        if ((*place->mId & 0xFF0000) != 0xFE0000) {
            entry->mLabel = place->mRegion;
            if (entry->mLabel == NULL) {
                if (place->mCountry != NULL) {
                    entry->mLabel = place->mCountry;
                } else {
                    entry->mLabel = place->mName;
                }
            }
        } else {
            entry->mLabel = L"\x25CF";
        }

        width = mWriter.CalcStringWidth(entry->mLabel);
        if (width > maxWidth) {
            entry->mLabelScaleX = maxWidth / width;
            width = maxWidth;
        } else {
            entry->mLabelScaleX = 1.0f;
        }

        entry->mNameScaleY = 1.0f;
        entry->mLabelScaleY = 1.0f;
        entry->mWidth = 20.0f + width;
        entry->mHeight = mRowHeight;
    }

    mBaseLayout = new ButtonGroup(arc, "base.brlyt", gButtonColors, false);
    mListLayout = new ButtonGroup(arc, "set_area1.brlyt", gButtonColors, false);
    mConfirmLayout = new ButtonGroup(arc, "set_area2.brlyt", gButtonColors, false);

    mUpButton = mListLayout->FindButton("up");
    if (mUpButton == NULL) {
        OSPanic("d_weather_address.cpp", 180, "up \x82\xAA\x82\xC8\x82\xA2\x82\xC5\x82\xB7\x21\x21\n");
    }

    mDownButton = mListLayout->FindButton("down");
    if (mDownButton == NULL) {
        OSPanic("d_weather_address.cpp", 185, "down \x82\xAA\x82\xC8\x82\xA2\x82\xC5\x82\xB7\x21\x21\n");
    }

    mBackButton = mListLayout->FindButton("back");
    if (mBackButton == NULL) {
        OSPanic("d_weather_address.cpp", 190, "back \x82\xAA\x82\xC8\x82\xA2\x82\xC5\x82\xB7\x21\x21\n");
    }

    mTitle = mListLayout->FindButton("text");
    if (mTitle == NULL) {
        OSPanic("d_weather_address.cpp", 195, "text \x82\xAA\x82\xC8\x82\xA2\x82\xC5\x82\xB7\x21\x21\n");
    }

    const wchar_t* otherText = gOtherRegionText[gLanguage];
    s32 len = wcslen(otherText) + 1;
    mOtherEntry.mPrev = NULL;
    mOtherEntry.mNext = NULL;
    mOtherEntry.mPlace = NULL;
    mOtherEntry.mName = new wchar_t[len];
    wcscpy(mOtherEntry.mName, otherText);
    mOtherEntry.mLabel = NULL;
    mOtherEntry.mX = sOtherEntryX;
    mOtherEntry.mY = sOtherEntryY;
    mOtherEntry.mWidth = GetScreenWidth();
    mOtherEntry.mHeight = mRowHeight;
    mOtherEntry.mNameScaleX = 1.0f;
    mOtherEntry.mNameScaleY = 1.0f;
    mOtherEntry.mLabelScaleX = 1.0f;
    mOtherEntry.mLabelScaleY = 1.0f;
    mOtherEntry.mLeft = 0.0f;
    mOtherEntry.mTop = 0.0f;
    mOtherEntry.mRight = 0.0f;
    mOtherEntry.mBottom = 0.0f;
    mOtherEntry.mHovered = FALSE;
    mOtherEntry.mWasHovered = FALSE;
    mOtherEntry.mHidden = FALSE;

    mUpArrowPos.y = 108.0f;
    mDownArrowPos.x = mUpArrowPos.x = 0.5f * (GetScreenWidth() - GetTexWidth(gCommonTpl, 0));
    mDownArrowPos.y = 388.0f - GetTexHeight(gCommonTpl, 0);

    CHANGE_STATE(&WeatherAddress::StateClose, 0);
}

WeatherAddress::~WeatherAddress() {
    if (mOtherEntry.mName != NULL) {
        delete[] mOtherEntry.mName;
    }

    if (mConfirmLayout != NULL) {
        delete mConfirmLayout;
    }

    if (mListLayout != NULL) {
        delete mListLayout;
    }

    if (mBaseLayout != NULL) {
        delete mBaseLayout;
    }

    if (mEntries != NULL) {
        AddressEntry* entry = mEntries;
        for (s32 i = 0; i < mNumPlaces; i++, entry++) {
            if (entry->mName != NULL) {
                delete[] entry->mName;
            }
        }

        delete[] mEntries;
    }
}

void WeatherAddress::Reset() {
    mBaseLayout->Reset();
    mListLayout->Reset();
    mConfirmLayout->Reset();
}

void WeatherAddress::Draw() {
    mBaseLayout->Draw();

    if (mDrawFunc) {
        SetDefaultGXState();
        SetOrthoProjection();
        (this->*mDrawFunc)();
    }

    if (mShowArrows) {
        SetDefaultGXState();
        SetOrthoProjection();
        if (!mUpButton->mLocked) {
            DrawTexture(gCommonTpl, 0, 1.0f, 1.0f, &mUpArrowPos, 0);
        }

        if (!mDownButton->mLocked) {
            DrawTexture(gCommonTpl, 0, 1.0f, 1.0f, &mDownArrowPos, 2);
        }
    }

    mActiveLayout->Draw();
}

void WeatherAddress::Calc() {
    mUp = FALSE;
    mDown = FALSE;
    mDragging = FALSE;
    mYes = FALSE;
    mNo = FALSE;
    mBaseLayout->Calc();
    mListLayout->Calc();
    mConfirmLayout->Calc();

    if (mState) {
        (this->*mState)(0);
    }
}

void WeatherAddress::SelectCurrentCity() {
    s32 index;

    mAreaId = gCurrentCityId & 0xFFFF0000;
    FIND_AREA(index);
    if (index < 0) {
        CHANGE_STATE(&WeatherAddress::StateArea, 0);
    } else {
        CHANGE_STATE(&WeatherAddress::StateCity, 0);
    }
}

void WeatherAddress::UpdateInput() {
    UpdateButtons(mActiveLayout, 40);

    if (gHoldAll & 0x400) {
        mDragging = TRUE;
    } else {
        if (CheckButtonHeld("down", 0x800) >= 0) {
            mDown = TRUE;
        } else if (gRepeatFastAll & 4) {
            mDown = TRUE;
            mDownButton->Press(1);
        }

        if (CheckButtonHeld("up", 0x800) >= 0) {
            mUp = TRUE;
        } else if (gRepeatFastAll & 8) {
            mUp = TRUE;
            mUpButton->Press(1);
        }
    }

    if (CheckButtonPressed("yes", 0x800) >= 0) {
        mYes = TRUE;
    }

    if (CheckButtonPressed("no", 0x800) >= 0) {
        mNo = TRUE;
    }
}

s32 WeatherAddress::BuildCityList() {
    s32 i;
    AddressEntry* entry;
    s32 index;

    mListHead = NULL;
    f32 y = 0.5f * mRowHeight;
    entry = mEntries;
    mNumEntries = 0;
    for (i = 0; i < mNumPlaces; i++, entry++) {
        entry->mPrev = NULL;
        entry->mNext = NULL;
    }

    FIND_AREA(index);
    if (index < 0) {
        return 0;
    }

    entry = &mEntries[index];
    mListHead = entry;
    mNumEntries++;
    entry->mX = mListX;
    entry->mY = y;

    AddressEntry* last = mListHead;
    y += mRowHeight;
    entry++;
    for (i = index + 1; i < mNumPlaces; i++, entry++) {
        if (mAreaId == (*entry->mPlace->mId & 0xFFFF0000)) {
            last->mNext = entry;
            entry->mPrev = last;
            last = entry;
            mNumEntries++;
            entry->mX = mListX;
            entry->mY = y;
            y += mRowHeight;
        }
    }

    last->mNext = &mOtherEntry;
    mOtherEntry.mPrev = last;
    mOtherEntry.mX = mListX;
    mOtherEntry.mY = y;
    mNumEntries++;
    mNumPages = mNumEntries - 3;
    return 0;
}

s32 WeatherAddress::BuildAreaList() {
    s32 i;
    AddressEntry* entry = mEntries;

    f32 y = 0.5f * mRowHeight;
    for (i = 0; i < mNumPlaces; i++, entry++) {
        entry->mPrev = NULL;
        entry->mNext = NULL;
    }

    AddressEntry* first = mEntries;
    mListHead = first;
    mNumEntries = 1;
    first->mX = mListX;
    first->mY = y;

    AddressEntry* last = mListHead;
    y += mRowHeight;
    entry = first + 1;
    for (i = 1; i < mNumPlaces; i++, entry++) {
        u32 id = *entry->mPlace->mId;
        if ((id & 0xFF000000) == (*first->mPlace->mId & 0xFF000000)) {
            BOOL found;
            AddressEntry* area = mListHead;
            while (TRUE) {
                if (area == NULL) {
                    found = FALSE;
                    break;
                }

                if ((*area->mPlace->mId & 0xFFFF0000) == (id & 0xFFFF0000)) {
                    found = TRUE;
                    break;
                }

                area = area->mNext;
            }

            if (!found) {
                last->mNext = entry;
                entry->mPrev = last;
                last = entry;
                mNumEntries++;
                entry->mX = mListX;
                entry->mY = y;
                y += mRowHeight;
            }
        }
    }

    mNumPages = mNumEntries - 3;
    if (mNumPages <= 0) {
        return 0;
    }

    s32 index = 0;
    for (entry = mListHead; entry != NULL; entry = entry->mNext, index++) {
        if (mAreaId == (*entry->mPlace->mId & 0xFFFF0000)) {
            if (index >= mNumPages - 1) {
                index = mNumPages - 1;
            }
            return index;
        }
    }

    return 0;
}

void WeatherAddress::DrawCityList() {
    AddressEntry* entry = mListHead;
    f32 offset = mScroll + mListTop;
    nw4r::ut::Color color;
    Vec quad[4];

    SetScaledScissor(0, 103, GetScreenWidth(), 353);
    quad[0].z = 0.0f;
    quad[1].z = 0.0f;
    quad[2].z = 0.0f;
    quad[3].z = 0.0f;
    for (; entry != NULL; entry = entry->mNext) {
        if (!entry->mHidden) {
            quad[0].x = entry->mLeft;
            quad[1].x = entry->mLeft;
            quad[2].x = entry->mRight;
            quad[3].x = entry->mRight;
            quad[0].y = entry->mTop;
            quad[3].y = entry->mTop;
            quad[1].y = entry->mBottom;
            quad[2].y = entry->mBottom;

            const Color* src = &sListColor;
            if (entry->mHovered) {
                src = &gHighlightColor;
            }

            color.r = src->r;
            color.g = src->g;
            color.b = src->b;
            color.a = src->a;
            DrawQuad(quad, &color);
        }
    }

    mWriter.SetDrawFlag(0x111);
    mWriter.SetupGX();
    mWriter.SetTextColor(nw4r::ut::Color(*(GXColor*)&sTextColor));
    for (entry = mListHead; entry != NULL; entry = entry->mNext) {
        if (!entry->mHidden) {
            mWriter.SetScale(entry->mNameScaleX, entry->mNameScaleY);
            mWriter.SetCursor(entry->mX, entry->mY + offset);
            mWriter.Print(entry->mName);
        }
    }

    SetScaledScissor(0, 0, GetScreenWidth(), 456);
}

void WeatherAddress::DrawAreaList() {
    AddressEntry* entry = mListHead;
    f32 offset = mScroll + mListTop;
    nw4r::ut::Color color;
    Vec quad[4];

    SetScaledScissor(0, 103, GetScreenWidth(), 353);
    quad[0].z = 0.0f;
    quad[1].z = 0.0f;
    quad[2].z = 0.0f;
    quad[3].z = 0.0f;
    for (; entry != NULL; entry = entry->mNext) {
        if (!entry->mHidden) {
            quad[0].x = entry->mLeft;
            quad[1].x = entry->mLeft;
            quad[2].x = entry->mRight;
            quad[3].x = entry->mRight;
            quad[0].y = entry->mTop;
            quad[3].y = entry->mTop;
            quad[1].y = entry->mBottom;
            quad[2].y = entry->mBottom;

            const Color* src = &sListColor;
            if (entry->mHovered) {
                src = &gHighlightColor;
            }

            color.r = src->r;
            color.g = src->g;
            color.b = src->b;
            color.a = src->a;
            DrawQuad(quad, &color);
        }
    }

    mWriter.SetDrawFlag(0x111);
    mWriter.SetupGX();
    mWriter.SetTextColor(nw4r::ut::Color(*(GXColor*)&sTextColor));
    for (entry = mListHead; entry != NULL; entry = entry->mNext) {
        if (!entry->mHidden) {
            mWriter.SetScale(entry->mLabelScaleX, entry->mLabelScaleY);
            mWriter.SetCursor(entry->mX, entry->mY + offset);
            mWriter.Print(entry->mLabel);
        }
    }

    SetScaledScissor(0, 0, GetScreenWidth(), 456);
}

void WeatherAddress::DrawConfirm() {
    nw4r::ut::Color color(255, 255, 255, 64);
    Vec2 pos;
    Vec quad[4];

    pos.x = mListX;
    pos.y = 150.0f;
    quad[3].z = 0.0f;
    quad[2].z = 0.0f;
    quad[1].z = 0.0f;
    quad[0].z = 0.0f;
    quad[1].x = 0.0f;
    quad[0].x = 0.0f;
    f32 right = GetScreenWidth();
    quad[3].y = pos.y - 20.0f;
    quad[3].x = right;
    quad[2].x = right;
    quad[0].y = pos.y - 20.0f;
    quad[2].y = 20.0f + pos.y;
    quad[1].y = 20.0f + pos.y;
    DrawQuad(quad, (GXColor*)&sListColor);

    SetDefaultGXState();
    SetOrthoProjection();
    mWriter.SetDrawFlag(0x111);
    mWriter.SetupGX();
    mWriter.SetTextColor(nw4r::ut::Color(*(GXColor*)&sTextColor));
    mWriter.SetScale(mSelected->mNameScaleX, mSelected->mNameScaleY);
    mWriter.SetCursor(pos.x, pos.y);
    mWriter.Print(mSelected->mName);
}

void WeatherAddress::ScrollNormal() {
    switch (mScrollPhase) {
    case 0:
        mScrollPhase++;
        break;
    case -1:
        break;
    default:
        if (mNumPages > 1 && mDragging) {
            CHANGE_SCROLL(&WeatherAddress::ScrollDrag);
            return;
        }

        if (mDown) {
            if (mNumPages > 1 && mTopIndex < mNumPages - 1) {
                mTopIndex++;
                if (mTopIndex >= mNumPages) {
                    mTopIndex = mNumPages - 1;
                }
                PlaySE(48);
            }
        } else if (mUp) {
            if (mTopIndex > 0) {
                mTopIndex--;
                if (mTopIndex < 0) {
                    mTopIndex = 0;
                }
                PlaySE(48);
            }
        }

        if (mTopIndex == 0) {
            mUpButton->mLocked = TRUE;
            mUpButton->Release();
        } else {
            mUpButton->mLocked = FALSE;
        }

        if (mNumPages <= 0 || mTopIndex == mNumPages - 1) {
            mDownButton->mLocked = TRUE;
            mDownButton->Release();
        } else {
            mDownButton->mLocked = FALSE;
        }

        mScrollTarget = -(mRowHeight * mTopIndex);
        break;
    }

    SmoothApproach(&mScroll, mScrollTarget, 0.1f, 20.0f, 5.0f);
}

void WeatherAddress::ScrollDrag() {
    switch (mScrollPhase) {
    case -1:
        gCursorState[0] = 0;
        gCursorState[1] = 0;
        gCursorState[2] = 0;
        gCursorState[3] = 0;
        mShowArrows = FALSE;
        if (mScrollSpeed < 0.0f) {
            mTopIndex = (mRowHeight - mScroll) / mRowHeight;
        } else {
            mTopIndex = -mScroll / mRowHeight;
        }

        if (mTopIndex > mNumPages - 1) {
            mTopIndex = mNumPages - 1;
        } else if (mTopIndex < 0) {
            mTopIndex = 0;
        }

        mScrollTarget = -(mRowHeight * mTopIndex);
        return;
    case 0:
        mScrollPhase++;
        mScrollMax = 0.0f;
        mScrollSpeed = 0.0f;
        mScrollMin = -(mRowHeight * (mNumPages - 1));
        PlaySE(22);
        mShowArrows = TRUE;
    default:
        if (!mDragging) {
            CHANGE_SCROLL(&WeatherAddress::ScrollNormal);
            return;
        }

        for (s32 i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            gCursorState[i] = 0;
            if (gHold[i] & 0x400) {
                gCursorState[i] = 4;
                mScrollSpeed = 0.1f * gDragScroll[i];
                break;
            }
        }

        mScroll += mScrollSpeed;
        if (mScroll > mScrollMax) {
            mScroll = mScrollMax;
        } else if (mScroll < mScrollMin) {
            mScroll = mScrollMin;
        }

        if (mScroll >= mScrollMax) {
            mUpButton->mLocked = TRUE;
            mUpButton->Release();
            mDownButton->mLocked = FALSE;
        } else if (mScroll <= mScrollMin) {
            mUpButton->mLocked = FALSE;
            mDownButton->mLocked = TRUE;
            mDownButton->Release();
        } else {
            mUpButton->mLocked = FALSE;
            mDownButton->mLocked = FALSE;
        }
        break;
    }
}

void WeatherAddress::ChangeState(StateFunc state, s32 arg) {
    if (mState) {
        mPhase = -1;
        (this->*mState)(arg);
    }

    mPhase = 0;
    mState = state;
    if (mState) {
        (this->*mState)(arg);
    }
}

#define UPDATE_ENTRIES()                                                                                     \
    {                                                                                                        \
        AddressEntry* entry = mListHead;                                                                     \
        f32 offset = (mScroll + mListTop) - 20.0f;                                                           \
        f32 right = GetScreenWidth();                                                                        \
        for (; entry != NULL; entry = entry->mNext) {                                                        \
            BOOL hidden = FALSE;                                                                             \
            entry->mWasHovered = entry->mHovered;                                                            \
            entry->mHovered = FALSE;                                                                         \
            entry->mLeft = 0.0f;                                                                             \
            entry->mTop = entry->mY + offset;                                                                \
            entry->mRight = right;                                                                           \
            entry->mBottom = 40.0f + entry->mTop;                                                            \
            if (entry->mBottom < 0.0f || entry->mTop > mClipBottom) {                                        \
                hidden = TRUE;                                                                               \
            }                                                                                                \
            entry->mHidden = hidden;                                                                         \
        }                                                                                                    \
    }

BOOL WeatherAddress::UpdateCityList() {
    UPDATE_ENTRIES();

    for (AddressEntry* entry = mListHead; entry != NULL; entry = entry->mNext) {
        if (HitTest(entry) >= 0) {
            if (entry->mPlace != NULL) {
                CHANGE_STATE(&WeatherAddress::StateConfirm, (s32)entry);
                PlaySE(36);
            } else {
                CHANGE_STATE(&WeatherAddress::StateArea, 0);
                PlaySE(47);
            }
            return TRUE;
        }
    }

    return FALSE;
}

BOOL WeatherAddress::UpdateAreaList() {
    UPDATE_ENTRIES();

    for (AddressEntry* entry = mListHead; entry != NULL; entry = entry->mNext) {
        if (HitTest(entry) >= 0) {
            mAreaId = *entry->mPlace->mId & 0xFFFF0000;
            CHANGE_STATE(&WeatherAddress::StateCity, 0);
            PlaySE(35);
            return TRUE;
        }
    }

    return FALSE;
}

BOOL WeatherAddress::StateCity(s32 arg) {
    switch (mPhase) {
    case 0:
        mPhase++;
        CHANGE_SCROLL(&WeatherAddress::ScrollNormal);
        mTopIndex = BuildCityList();
        mActiveLayout = mListLayout;
        mScroll = mScrollTarget = -(mRowHeight * mTopIndex);
        mDrawFunc = &WeatherAddress::DrawCityList;
        if (gViewsCreated) {
            mBackButton->mLocked = FALSE;
        } else {
            mBackButton->mLocked = TRUE;
            mBackButton->Release();
        }
        mTitle->SetState(0);
        break;
    case -1:
        break;
    default:
        UpdateInput();
        switch (mPhase) {
        case 1:
            if (CheckButtonPressed("back", 0x800) >= 0) {
                mBackButton->mPressed = TRUE;
                PlaySE(38);
                gSettingResult = 4;
                CHANGE_STATE(&WeatherAddress::StateClose, 0);
                return;
            }

            if (mScrollState) {
                (this->*mScrollState)();
            }

            if (UpdateCityList()) {
                return;
            }
            return;
        case 2:
        default:
            if (mTimer != 0) {
                mTimer--;
                return;
            }

            gSettingResult = 4;
            CHANGE_STATE(&WeatherAddress::StateClose, 0);
            break;
        }
        break;
    }
}

BOOL WeatherAddress::StateArea(s32 arg) {
    s32 index;

    switch (mPhase) {
    case 0:
        mPhase++;
        CHANGE_SCROLL(&WeatherAddress::ScrollNormal);
        mTopIndex = BuildAreaList();
        mActiveLayout = mListLayout;
        mScroll = mScrollTarget = -(mRowHeight * mTopIndex);
        mDrawFunc = &WeatherAddress::DrawAreaList;
        mTitle->SetState(1);
        FIND_AREA(index);
        mCanGoBack = index >= 0;
        if (mCanGoBack) {
            mBackButton->mLocked = FALSE;
        } else {
            mBackButton->mLocked = TRUE;
            mBackButton->Release();
        }
        break;
    case -1:
        break;
    default:
        UpdateInput();
        switch (mPhase) {
        case 1:
            if (mCanGoBack && CheckButtonPressed("back", 0x800) >= 0) {
                mBackButton->mPressed = TRUE;
                PlaySE(38);
                mTimer = 20;
                mPhase++;
                return;
            }

            if (mScrollState) {
                (this->*mScrollState)();
            }

            if (UpdateAreaList()) {
                return;
            }
            return;
        case 2:
        default:
            if (mTimer != 0) {
                mTimer--;
                return;
            }

            CHANGE_STATE(&WeatherAddress::StateCity, 0);
            break;
        }
        break;
    }
}

BOOL WeatherAddress::StateConfirm(s32 arg) {
    switch (mPhase) {
    case 0:
        mPhase++;
        mActiveLayout = mConfirmLayout;
        mDrawFunc = &WeatherAddress::DrawConfirm;
        mSelected = (AddressEntry*)arg;
        if (mSelected == NULL) {
            mSelected = mEntries;
        }
        mConfirmLayout->Reset();
        mScroll = 0.0f;
        mScrollTarget = 0.0f;
        CHANGE_SCROLL(&WeatherAddress::ScrollNormal);
        break;
    case -1:
        break;
    default:
        UpdateInput();
        switch (mPhase) {
        case 1:
            if (mYes) {
                PlaySE(37);
                mPhase = 2;
                mTimer = 0;
                mConfirmLayout->FindButton("yes")->mPressed = TRUE;
                return;
            }

            if (mNo) {
                PlaySE(38);
                mPhase = 3;
                mTimer = 12;
                mConfirmLayout->FindButton("no")->mPressed = TRUE;
                return;
            }
            break;
        case 2:
            if (mTimer != 0) {
                mTimer--;
                return;
            }

            gCurrentCityId = *mSelected->mPlace->mId;
            if (!gViewsCreated) {
                gSettingResult = 2;
            } else {
                gSettingResult = 4;
                UpdateCurrentCity();
            }
            CHANGE_STATE(&WeatherAddress::StateClose, 0);
            break;
        default:
            if (mTimer != 0) {
                mTimer--;
                return;
            }

            CHANGE_STATE(&WeatherAddress::StateCity, 0);
            break;
        }
        break;
    }
}

BOOL WeatherAddress::StateClose(s32 arg) {
    switch (mPhase) {
    case 0:
        mPhase++;
        CHANGE_SCROLL(&WeatherAddress::ScrollNormal);
        break;
    case -1:
        break;
    default:
        break;
    }
}

s32 WeatherAddress::HitTest(AddressEntry* entry) {
    for (s32 i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }

        if (valid) {
            f32 y = gPointerY[i][0];
            f32 x = gPointerX[i][0];
            if (y > mClipTop && y < mClipBottom && x > entry->mLeft && x < entry->mRight && y > entry->mTop &&
                y < entry->mBottom) {
                entry->mHovered = TRUE;
                if (!entry->mWasHovered) {
                    PlaySE(46);
                }

                if (gTrig[i] & 0x800) {
                    return i;
                }
            }
        }
    }

    return -1;
}
