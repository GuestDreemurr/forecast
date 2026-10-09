#include <channel/LayoutObj.h>
#include <channel/System.h>
#include <cstdio>
#include <channel/TextTagProcessor.h>
#include <nw4r/lyt.h>
#include <nw4r/ut/ut_CharWriter.h>
#include <nw4r/ut/ut_TextWriterBase.h>
#include <nw4r/ut/ut_Color.h>

const wchar_t* gAmText = L"a.m.";
const wchar_t* gPmText = L"p.m.";

static const GXColor sColorBase0 = {0x00, 0x00, 0x00, 0xFF};
static const GXColor sColorBase1 = {0xC8, 0x8C, 0x8C, 0xFF};
static const GXColor sColorAlt0 = {0x00, 0x00, 0x00, 0xFF};
static const GXColor sColorAlt1 = {0x85, 0xE6, 0xFF, 0xFF};
static const GXColor sColorSky0 = {0x00, 0x00, 0x00, 0xFF};
static const GXColor sColorSky1 = {0xFF, 0xFF, 0xFF, 0xFF};
static const GXColor sColorNight0 = {0x00, 0x00, 0x00, 0xFF};
static const GXColor sColorNight1 = {0x00, 0x56, 0xA7, 0xFF};
static const GXColor sColorDark0 = {0x00, 0x00, 0x00, 0xFF};
static const GXColor sColorDark1 = {0x62, 0x23, 0x23, 0xFF};
static const GXColor sColorSel0 = {0x8C, 0x00, 0x00, 0xFF};
static const GXColor sColorSel1 = {0xFF, 0xFF, 0xFF, 0xFF};

// The text colors come before the pane colors. The two sets hold the same values, and with
// them the other way round Draw loads each set from the other's slots.
static const GXColor sColorTextIdle = {0xD8, 0xD8, 0xD8, 0xFF};
static const GXColor sColorTextActive = {0x00, 0x00, 0x00, 0xFF};
static const GXColor sColorTextSel = {0xD8, 0xD8, 0xD8, 0xFF};
static const GXColor sColorPaneIdle = {0xD8, 0xD8, 0xD8, 0xFF};
static const GXColor sColorPaneActive = {0x00, 0x00, 0x00, 0xFF};
static const GXColor sColorPaneSel = {0xD8, 0xD8, 0xD8, 0xFF};
static const GXColor sTagRed = {0x8C, 0x00, 0x00, 0xFF};
static const GXColor sTagBlue = {0x00, 0x00, 0x8C, 0xFF};

// Highlight colors (TEV color 0 and 1) for each item type. Defined in WeatherNormal.cpp, where
// they sit in .rodata.
extern const GXColor gTypeHighlightColor0[10];
extern const GXColor gTypeHighlightColor1[10];

static u8 sFlag;

// The game's lyt::Pane has a different layout from the one in nw4r/lyt/lyt_pane.h, so these
// helpers reach its fields by offset.

static inline const char* GetPaneName(nw4r::lyt::Pane* pane) {
    return (const char*)pane + 0xB4;
}

// Not static: LayoutButton calls the out-of-line copies of these two that this unit emits.
inline void SetPaneVisible(nw4r::lyt::Pane* pane, bool visible) {
    u8* flag = (u8*)pane + 0xCF;
    *flag = (*flag & 0xFE) | visible;
}

inline const char* GetPaneUserData(nw4r::lyt::Pane* pane) {
    return (const char*)pane + 0xC4;
}

GXColor LerpColor(const GXColor* a, const GXColor* b, s32 num, s32 den);

// Sets the tag processor of the pane if it is a text box
static inline void SetTextBoxTagProcessor(nw4r::lyt::Pane* pane, nw4r::ut::WideTagProcessor* processor) {
    nw4r::lyt::TextBox* textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(pane);
    if (textBox != NULL) {
        textBox->SetTagProcessor(processor);
    }
}

// The original expands the recursion three levels deep before it calls itself, both here and
// where the constructor uses it (SetPaneTagProcessorExpanded). The expansion is written out
// here: going through the inline adds an inlining level that changes the register allocation.
//
// This unit is built with -ipa file, which puts the out-of-line copies of inline functions
// right after the first function that uses them, in the order it uses them. This function
// places the LinkList helpers that Draw calls, so it must come before
// SetPaneTagProcessorExpanded and use the postfix increment that Draw uses.
void SetPaneTagProcessor(nw4r::lyt::Pane* pane, nw4r::ut::WideTagProcessor* processor) {
    SetTextBoxTagProcessor(pane, processor);

    nw4r::lyt::PaneList& list1 = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it1 = list1.GetBeginIter(); it1 != list1.GetEndIter(); it1++) {
        nw4r::lyt::Pane* child1 = &*it1;
        SetTextBoxTagProcessor(child1, processor);

        nw4r::lyt::PaneList& list2 = child1->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it2 = list2.GetBeginIter(); it2 != list2.GetEndIter(); it2++) {
            nw4r::lyt::Pane* child2 = &*it2;
            SetTextBoxTagProcessor(child2, processor);

            nw4r::lyt::PaneList& list3 = child2->GetChildList();
            for (nw4r::lyt::PaneList::Iterator it3 = list3.GetBeginIter(); it3 != list3.GetEndIter(); it3++) {
                SetPaneTagProcessor(&*it3, processor);
            }
        }
    }
}

static inline void SetPaneTagProcessorExpanded(nw4r::lyt::Pane* pane, nw4r::ut::WideTagProcessor* processor) {
    SetTextBoxTagProcessor(pane, processor);

    nw4r::lyt::PaneList& list1 = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it1 = list1.GetBeginIter(); it1 != list1.GetEndIter(); ++it1) {
        nw4r::lyt::Pane* child1 = &*it1;
        SetTextBoxTagProcessor(child1, processor);

        nw4r::lyt::PaneList& list2 = child1->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it2 = list2.GetBeginIter(); it2 != list2.GetEndIter(); ++it2) {
            nw4r::lyt::Pane* child2 = &*it2;
            SetTextBoxTagProcessor(child2, processor);

            nw4r::lyt::PaneList& list3 = child2->GetChildList();
            for (nw4r::lyt::PaneList::Iterator it3 = list3.GetBeginIter(); it3 != list3.GetEndIter(); ++it3) {
                SetPaneTagProcessor(&*it3, processor);
            }
        }
    }
}

void SetTextBoxColors(nw4r::lyt::TextBox* textBox, nw4r::ut::Color top, nw4r::ut::Color bottom);

// Colors every text box under the pane, except below panes tagged 'F'
void SetPaneTextColor(nw4r::lyt::Pane* pane, const GXColor* color) {
    char tag = GetPaneUserData(pane)[0];
    if (tag == 'F' || tag == 'f') {
        return;
    }

    nw4r::lyt::TextBox* textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(pane);
    if (textBox != NULL) {
        nw4r::ut::Color c = *color;
        SetTextBoxColors(textBox, c, c);
    }

    nw4r::lyt::PaneList& list = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
        SetPaneTextColor(&*it, color);
    }
}

// Defined after SetPaneTextColor, so that call is not inlined (the linker keeps the copy from
// WeatherAround). Its Color copy places the out-of-line GXColor assignment Draw calls.
inline void SetTextBoxColors(nw4r::lyt::TextBox* textBox, nw4r::ut::Color top, nw4r::ut::Color bottom) {
    // TextBox::mTextColors is protected
    nw4r::ut::Color* colors = (nw4r::ut::Color*)((u8*)textBox + 0xD8);
    colors[0] = top;
    colors[1] = bottom;
}

// Sets the TEV color (which = 0: color 0, 1: color 1) of every material under the pane,
// except below panes tagged 'F'
void SetPaneTevColor(nw4r::lyt::Pane* pane, const GXColor* color, s32 which) {
    char tag = GetPaneUserData(pane)[0];
    if (tag == 'F' || tag == 'f') {
        return;
    }

    nw4r::lyt::Material* material = pane->FindMaterialByName(GetPaneName(pane), true);
    if (material != NULL) {
        switch (which) {
        case 0:
            material->SetColorElement(4, color->r);
            material->SetColorElement(5, color->g);
            material->SetColorElement(6, color->b);
            break;
        case 1:
            material->SetColorElement(8, color->r);
            material->SetColorElement(9, color->g);
            material->SetColorElement(10, color->b);
            break;
        }
    }

    nw4r::lyt::PaneList& list = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
        SetPaneTevColor(&*it, color, which);
    }
}

// Sets TEV color 1's alpha on the pane's material
static inline void SetMaterialTevAlpha(nw4r::lyt::Pane* pane, u8 alpha) {
    nw4r::lyt::Material* material = pane->FindMaterialByName(GetPaneName(pane), true);
    if (material != NULL) {
        material->SetColorElement(11, alpha);
    }
}

// Sets TEV color 1's alpha of every material under the pane. The original expands the
// recursion three levels deep before it calls itself.
void SetPaneTevAlpha(nw4r::lyt::Pane* pane, s32 alpha) {
    SetMaterialTevAlpha(pane, alpha);

    nw4r::lyt::PaneList& list1 = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it1 = list1.GetBeginIter(); it1 != list1.GetEndIter(); ++it1) {
        nw4r::lyt::Pane* child1 = &*it1;
        SetMaterialTevAlpha(child1, alpha);

        nw4r::lyt::PaneList& list2 = child1->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it2 = list2.GetBeginIter(); it2 != list2.GetEndIter(); ++it2) {
            nw4r::lyt::Pane* child2 = &*it2;
            SetMaterialTevAlpha(child2, alpha);

            nw4r::lyt::PaneList& list3 = child2->GetChildList();
            for (nw4r::lyt::PaneList::Iterator it3 = list3.GetBeginIter(); it3 != list3.GetEndIter(); ++it3) {
                SetPaneTevAlpha(&*it3, alpha);
            }
        }
    }
}

#pragma auto_inline on
TextTagProcessor::TextTagProcessor() {}
#pragma auto_inline reset

TextTagProcessor::~TextTagProcessor() {}

TextTagProcessor::Operation TextTagProcessor::Process(u16 ch, ContextType* pCtx) {
    switch (ch) {
    case 1:
        mSavedColor = pCtx->writer->GetTextColor();
        pCtx->writer->SetTextColor(sTagRed);
        return OPERATION_NO_CHAR_SPACE;
    case 2:
        mSavedColor = pCtx->writer->GetTextColor();
        pCtx->writer->SetTextColor(sTagBlue);
        return OPERATION_NO_CHAR_SPACE;
    case 9:
        pCtx->writer->SetTextColor(mSavedColor);
        return OPERATION_NO_CHAR_SPACE;
    default:
        return TagProcessorBase<wchar_t>::Process(ch, pCtx);
    }
}

TextTagProcessor::Operation TextTagProcessor::CalcRect(nw4r::ut::Rect* pRect, u16 ch, ContextType* pCtx) {
    switch (ch) {
    case 1:
    case 2:
    case 9:
        return OPERATION_NO_CHAR_SPACE;
    default:
        return TagProcessorBase<wchar_t>::CalcRect(pRect, ch, pCtx);
    }
}

// Scales the pane horizontally, if there is one
static inline void ScalePaneX(nw4r::lyt::Pane* pane, f32 scaleX) {
    if (pane != NULL) {
        nw4r::math::VEC2 scale = pane->GetScale();
        scale.x = scale.x * scaleX;
        pane->SetScale(scale);
    }
}

// Shrinks the pane horizontally to undo the widescreen stretch
static inline void ScalePaneWidescreen(nw4r::lyt::Pane* pane) {
    nw4r::math::VEC2 scale = pane->GetScale();
    scale.x = scale.x * (1.0f / (gWidescreen ? 1.3684211f : 1.0f));
    pane->SetScale(scale);
}

LayoutObjItem::LayoutObjItem(nw4r::lyt::Pane* rootPane, nw4r::lyt::DrawInfo* drawInfo,
                             TextTagProcessor* tagProcessor, s32 arg)
    : mPane(rootPane), mDrawInfo(drawInfo) {
    char name[128];

    sprintf(name, "%sB", GetPaneName(mPane));
    mPaneB = mPane->FindPaneByName(name, true);
    if (mPaneB == NULL) {
        mPaneB = mPane;
    }

    sprintf(name, "%sR", GetPaneName(mPane));
    nw4r::lyt::Pane* pane = mPane->FindPaneByName(name, true);
    if (pane == NULL) {
        pane = mPaneB;
    }

    mOrigTrans = mPane->GetTranslate();
    mRect = pane->GetPaneRect(*mDrawInfo);
    mRect.left += mOrigTrans.x;
    mRect.right += mOrigTrans.x;
    mRect.top += mOrigTrans.y;
    mRect.bottom += mOrigTrans.y;
    if (pane != mPane) {
        nw4r::math::VEC3 trans = pane->GetTranslate();
        mRect.left += trans.x;
        mRect.right += trans.x;
        mRect.top += trans.y;
        mRect.bottom += trans.y;
    }

    unk44 = 0;
    unk45 = 0;
    unk46 = 0;
    unk48 = 0;
    unk47 = 0;
    unk49 = 0;
    unk4A = 0;
    unk4B = 0;
    unk4C = NULL;
    unk50 = NULL;
    unk3C = 0;
    mType = 0;

    for (s32 i = 0; i < 8; i++) {
        switch (GetPaneUserData(mPane)[i]) {
        case 'D':
        case 'd':
            unk4A = 1;
            break;
        case 'F':
        case 'f':
            unk3C = 1;
            break;
        case 'C':
        case 'c':
            i++;
            if (i < 8) {
                s32 digit = GetPaneUserData(mPane)[i] - '0';
                if (digit >= 0 && digit <= 9) {
                    mType = digit;
                }
            }
            break;
        }
    }

    sprintf(name, "%sI", GetPaneName(mPane));
    pane = mPane->FindPaneByName(name, true);
    if (pane == NULL) {
        mPaneI = NULL;
    } else {
        sprintf(name, "%s%s", GetPaneName(pane), GetLanguageCode());
        mPaneI = pane->FindPaneByName(name, true);
        if (mPaneI == NULL) {
            mPaneI = pane;
        } else {
            nw4r::lyt::PaneList& list = pane->GetChildList();
            for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
                if (&*it != mPaneI) {
                    SetPaneVisible(&*it, false);
                }
            }
        }
    }

    sprintf(name, "%sT", GetPaneName(mPane));
    pane = mPane->FindPaneByName(name, true);
    if (pane == NULL) {
        mPaneT = NULL;
    } else {
        sprintf(name, "%s%s", GetPaneName(pane), GetLanguageCode());
        mPaneT = pane->FindPaneByName(name, true);
        nw4r::lyt::PaneList& list = pane->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
            if (&*it != mPaneT) {
                SetPaneVisible(&*it, false);
            }
        }
    }

    if (unk3C && mPaneT != NULL) {
        SetPaneTagProcessorExpanded(mPaneT, tagProcessor);
    }

    sprintf(name, "%sF", GetPaneName(mPane));
    pane = mPane->FindPaneByName(name, true);
    if (pane == NULL) {
        mPaneF0 = NULL;
        mPaneF1 = NULL;
    } else {
        sprintf(name, "%sF0", GetPaneName(mPane));
        mPaneF0 = pane->FindPaneByName(name, true);
        sprintf(name, "%sF1", GetPaneName(mPane));
        mPaneF1 = pane->FindPaneByName(name, true);
    }

    sprintf(name, "%sM", GetPaneName(mPane));
    pane = mPane->FindPaneByName(name, true);
    if (pane == NULL) {
        mPaneM = NULL;
    } else {
        sprintf(name, "%s%s", GetPaneName(pane), GetLanguageCode());
        mPaneM = pane->FindPaneByName(name, true);
        nw4r::lyt::PaneList& list = pane->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
            if (&*it != mPaneM) {
                SetPaneVisible(&*it, false);
            }
        }
    }

    if (unk3C && mPaneM != NULL) {
        SetPaneTagProcessorExpanded(mPaneM, tagProcessor);
    }

    if (gWidescreen && arg == 0) {
        f32 scaleX = 1.0f / (gWidescreen ? 1.3684211f : 1.0f);
        ScalePaneX(mPaneT, scaleX);
        ScalePaneX(mPaneI, scaleX);
        ScalePaneX(mPaneF0, scaleX);
        ScalePaneX(mPaneF1, scaleX);
        ScalePaneX(mPaneM, scaleX);
    }

    nw4r::lyt::TextBox* textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(mPaneB);
    if (textBox != NULL) {
        if (gWidescreen) {
            ScalePaneWidescreen(textBox);
        }
        SetPaneVisible(textBox, true);
        SetPaneTagProcessorExpanded(textBox, tagProcessor);
    }

    unk7C = -1;
    unk80 = -1;
    unk84 = NULL;
    unk88 = NULL;
    unk8C = NULL;
    unk90 = NULL;
    unk94 = NULL;
    mSlideOffset = 0.0f;
    mAlpha = 0xFF;
    unk5C = 0xFF;
    unk60 = 0;
    unk64 = 0;
    unk68 = 0;
    unk6C = 0;
    unk70 = 0;
    unk74 = 0;
}

// Recursion written out three levels deep, like the original
static inline void MarkTreeLeaf(LayoutObjItem* item) {
    if (item->unk3C == 0 && item->unk4A == 0 && item->unk49 == 0) {
        item->unk60 = 1;
        if (item->unk4C != NULL) {
            item->unk4C->MarkTree();
        }
        if (item->unk50 != NULL) {
            item->unk50->MarkTree();
        }
    }
}

static inline void MarkTreeMid(LayoutObjItem* item) {
    if (item->unk3C == 0 && item->unk4A == 0 && item->unk49 == 0) {
        item->unk60 = 1;
        if (item->unk4C != NULL) {
            MarkTreeLeaf(item->unk4C);
        }
        if (item->unk50 != NULL) {
            MarkTreeLeaf(item->unk50);
        }
    }
}

static inline void MarkTreeTop(LayoutObjItem* item) {
    if (item->unk3C == 0 && item->unk4A == 0 && item->unk49 == 0) {
        item->unk60 = 1;
        if (item->unk4C != NULL) {
            MarkTreeMid(item->unk4C);
        }
        if (item->unk50 != NULL) {
            MarkTreeMid(item->unk50);
        }
    }
}

void LayoutObjItem::Update() {
    if (!unk49) {
        f32 shake;
        if (unk6C > 0 && mPaneF0 == NULL) {
            if (unk46) {
                shake = 12.0f;
                if (unk4C != NULL) {
                    MarkTreeTop(unk4C);
                }
                if (unk50 != NULL) {
                    MarkTreeTop(unk50);
                }
            } else {
                s32 n = unk6C;
                if (n > 16) {
                    n = 16;
                }
                f32 t = 1.5708f * (f32)(16 - n);
                t *= 0.0625f;
                shake = unk4B ? 4.0f : 18.0f;
                shake *= 1.0f - nw4r::math::SinFIdx(40.743664f * t);
            }
        } else {
            shake = 0.0f;
        }

        nw4r::math::VEC3 pos = mOrigTrans + nw4r::math::VEC3(0.0f, mSlideOffset - shake, 0.0f);
        mPane->SetTranslate(pos);

        if (unk60) {
            if (unk64 < 12) {
                unk64++;
            }
            unk60 = 0;
        } else if (unk64 > 0) {
            unk64--;
        }

        if (unk68) {
            if (unk6C > 0) {
                unk6C--;
            }
            if (unk6C == 0) {
                unk68 = 0;
            }
        } else if (unk6C > 1) {
            unk6C--;
        }

        if (unk6C > 10) {
            unk64 = 12;
        }
    }
}

#pragma dont_inline on
void LayoutObjItem::Draw() {
    nw4r::lyt::Pane* textPane = NULL;
    if (!unk49) {
        if (!unk3C) {
            SetPaneTevAlpha(mPane, mAlpha);
        }

        if (mPaneT != NULL) {
            nw4r::lyt::PaneList& list = mPaneT->GetChildList();
            textPane = mPaneT;
            s32 i = 0;
            for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); i++, it++) {
                if (unk70 == i) {
                    textPane = &*it;
                    break;
                }
            }

            SetPaneVisible(textPane, true);
            for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
                if (&*it != textPane) {
                    SetPaneVisible(&*it, false);
                }
            }
        }

        if (mPaneM != NULL) {
            nw4r::lyt::Pane* selected = GetChildPane(unk7C);
            nw4r::lyt::PaneList& list = mPaneM->GetChildList();
            for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
                SetPaneVisible(&*it, false);
            }

            if (selected != NULL) {
                nw4r::lyt::PaneList& children = selected->GetChildList();
                s32 j = 0;
                SetPaneVisible(selected, true);
                for (nw4r::lyt::PaneList::Iterator it = children.GetBeginIter(); it != children.GetEndIter();
                     j++, it++) {
                    SetPaneVisible(&*it, unk80 == j);
                }
            }
        }

        if (!unk3C) {
            s32 alphaA, alphaB, alphaC;
            if (mType == 8 || mType == 9) {
                alphaA = 0x80;
                alphaB = 255;
                alphaC = 255;
            } else {
                alphaA = mAlpha * unk5C / 255;
                s32 clamped = unk5C + 128 > 255 ? 255 : unk5C + 128;
                alphaB = mAlpha * clamped / 255;
                alphaC = alphaB;
            }

            if (mPaneB != mPane) {
                GXColor color0, color1, base0, base1;
                if (mType == 5) {
                    base0 = sColorSky0;
                    base1 = sColorSky1;
                } else if (mType == 8 || mType == 9) {
                    if (sFlag) {
                        base0 = sColorNight0;
                        base1 = sColorNight1;
                    } else {
                        base0 = sColorDark0;
                        base1 = sColorDark1;
                    }
                } else {
                    if (sFlag || mType == 7) {
                        base0 = sColorAlt0;
                        base1 = sColorAlt1;
                    } else {
                        base0 = sColorBase0;
                        base1 = sColorBase1;
                    }
                }

                if (unk74) {
                    color0 = sColorSel0;
                    color1 = sColorSel1;
                } else if (unk46 && unk6C > 0) {
                    color0 = gTypeHighlightColor0[mType];
                    color1 = gTypeHighlightColor1[mType];
                } else if (unk64 > 0) {
                    color0 = LerpColor(&base0, &gTypeHighlightColor0[mType], unk64, 12);
                    color1 = LerpColor(&base1, &gTypeHighlightColor1[mType], unk64, 12);
                } else if (unk6C > 0) {
                    color0 = gTypeHighlightColor0[mType];
                    color1 = gTypeHighlightColor1[mType];
                } else {
                    color0 = base0;
                    color1 = base1;
                }

                if (unk5C < 255) {
                    GXColor white0 = {0xFF, 0xFF, 0xFF, 0x00};
                    color0 = LerpColor(&color0, &white0, 255 - unk5C, 255);
                    GXColor white1 = {0xFF, 0xFF, 0xFF, 0x00};
                    color1 = LerpColor(&color1, &white1, 255 - unk5C, 255);
                }

                SetPaneTevColor(mPaneB, &color0, 0);
                SetPaneTevColor(mPaneB, &color1, 1);
                SetPaneTevAlpha(mPaneB, alphaA);
            }

            // With unk4A set, color is left uninitialized here and below (as in the original)
            if (mPaneI != NULL && GetPaneUserData(mPaneI)[0] != 'F') {
                GXColor color;
                if (unk74) {
                    color = sColorPaneSel;
                } else if (unk4A) {
                    alphaB = 0;
                } else if (unk46 && unk6C > 0) {
                    color = sColorPaneActive;
                } else if (unk64 > 0) {
                    color = LerpColor(&sColorPaneIdle, &sColorPaneActive, unk64, 12);
                } else if (unk6C > 0) {
                    color = sColorPaneActive;
                } else {
                    color = sColorPaneIdle;
                }

                SetPaneTevColor(mPaneI, &color, 0);
                SetPaneTevColor(mPaneI, &color, 1);
                SetPaneTevAlpha(mPaneI, alphaB);
            }

            if (textPane != NULL) {
                GXColor color;
                if (unk74) {
                    color = sColorTextSel;
                } else if (unk4A) {
                    alphaC = 0;
                } else if (unk46 && unk6C > 0) {
                    color = sColorTextActive;
                } else if (unk64 > 0) {
                    color = LerpColor(&sColorTextIdle, &sColorTextActive, unk64, 12);
                } else if (unk6C > 0) {
                    color = sColorTextActive;
                } else {
                    color = sColorTextIdle;
                }

                color.a = alphaC;
                SetPaneTextColor(textPane, &color);
                mTextColor = color;
            }

            if (mPaneF0 != NULL && mPaneF1 != NULL) {
                if (unk6C > 0) {
                    SetPaneVisible(mPaneF0, false);
                    SetPaneVisible(mPaneF1, true);
                } else {
                    SetPaneVisible(mPaneF0, true);
                    SetPaneVisible(mPaneF1, false);
                }
            }
        }

        mPane->Draw(*mDrawInfo);
    }
}
#pragma dont_inline reset

GXColor LerpColor(const GXColor* a, const GXColor* b, s32 num, s32 den) {
    GXColor c;
    c.r = (a->r * (den - num) + b->r * num) / den;
    c.g = (a->g * (den - num) + b->g * num) / den;
    c.b = (a->b * (den - num) + b->b * num) / den;
    return c;
}

void LayoutObjItem::MarkTree() {
    if (unk3C == 0 && unk4A == 0 && unk49 == 0) {
        unk60 = 1;
        if (unk4C != NULL) {
            MarkTreeMid(unk4C);
        }
        if (unk50 != NULL) {
            MarkTreeMid(unk50);
        }
    }
}

nw4r::lyt::Pane* LayoutObjItem::GetChildPane(s32 index) {
    nw4r::lyt::Pane* result = NULL;
    s32 i = 0;
    nw4r::lyt::PaneList& list = mPaneM->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it, i++) {
        if (index == i) {
            result = &*it;
            break;
        }
    }
    return result;
}

// Same as Reset, but the constructor's copy reads mItems directly
inline void LayoutObj::InitState() {
    for (s32 i = 0; i < mNumItems; i++) {
        LayoutObjItem* item = mItems[i];
        item->mSlideOffset = 0.0f;
        item->mAlpha = 0xFF;
        item->unk5C = 0xFF;
        item->unk60 = 0;
        item->unk64 = 0;
        item->unk68 = 0;
        item->unk6C = 0;
        item->unk70 = 0;
        item->unk74 = 0;
    }

    mSlidingOut = FALSE;
    mSlideFrames = 15;
    mSlideFrame = 0;
}

LayoutObj::LayoutObj(const void* archive, const char* layoutName, s32 arg) {
    mResAccessor = new nw4r::lyt::ArcResourceAccessor;
    mResAccessor->Attach((void*)archive, "arc");

    mLayout = new nw4r::lyt::Layout;
    void* layoutBin = mResAccessor->GetResource(0, layoutName, NULL);
    mLayout->Build(layoutBin, mResAccessor);

    mDrawInfo = new nw4r::lyt::DrawInfo;
    mDrawInfo->SetViewRect(mLayout->GetLayoutRect());

    nw4r::math::MTX34 mtx;
    PSMTXIdentity(mtx);
    mDrawInfo->SetViewMtx(mtx);

    mTagProcessor = new TextTagProcessor;

    nw4r::lyt::PaneList& panes = mLayout->GetRootPane()->GetChildList();
    mNumItems = 0;
    for (nw4r::lyt::PaneList::Iterator it = panes.GetBeginIter(); it != panes.GetEndIter(); ++it) {
        if (mNumItems >= 0x40) {
            break;
        }

        mItems[mNumItems++] = new LayoutObjItem(&*it, mDrawInfo, mTagProcessor, arg);
    }

    InitState();
}

LayoutObj::~LayoutObj() {
    for (s32 i = 0; i < mNumItems; i++) {
        LayoutObjItem* item = GetItem(i);
        if (item != NULL) {
            if (item->unk88 != NULL) {
                MEM1Free(item->unk88);
            }
            if (item->unk84 != NULL) {
                MEM1Free(item->unk84);
            }
            ::operator delete(item);
        }
    }

    delete mTagProcessor;
    delete mDrawInfo;
    delete mLayout;
    mResAccessor->Detach();
    delete mResAccessor;
}

void LayoutObj::Reset() {
    for (s32 i = 0; i < mNumItems; i++) {
        LayoutObjItem* item = GetItem(i);
        item->mSlideOffset = 0.0f;
        item->mAlpha = 0xFF;
        item->unk5C = 0xFF;
        item->unk60 = 0;
        item->unk64 = 0;
        item->unk68 = 0;
        item->unk6C = 0;
        item->unk70 = 0;
        item->unk74 = 0;
    }
    mSlidingOut = FALSE;
    mSlideFrames = 15;
    mSlideFrame = 0;
}

inline void LayoutObj::ApplyAlpha(s32 alpha) {
    for (s32 i = 0; i < mNumItems; i++) {
        GetItem(i)->mAlpha = alpha;
    }
}

void LayoutObj::Calc() {
    for (s32 i = 0; i < mNumItems; i++) {
        mItems[i]->Update();
    }

    if (mSlidingOut) {
        if (mSlideFrame < mSlideFrames) {
            mSlideFrame++;
        }
    } else if (mSlideFrame > 0) {
        mSlideFrame--;
    }

    f32 height;
    if (mNumItems > 0) {
        height = nw4r::math::FAbs(mItems[0]->mRect.top - mItems[0]->mRect.bottom);
    } else {
        height = 0.0f;
    }

    f32 offset = height * mSlideFrame / mSlideFrames;
    for (s32 i = 0; i < mNumItems; i++) {
        f32 value = offset * (mItems[i]->mPane->GetTranslate().y > 0.0f ? 1 : -1);
        if (!mItems[i]->unk48) {
            mItems[i]->mSlideOffset = value;
        }
    }

    if (mFadingOut) {
        if (mFadeFrame < mFadeFrames) {
            mFadeFrame++;
        }
    } else if (mFadeFrame > 0) {
        mFadeFrame--;
    }

    s32 alpha = 255 - mFadeFrame * 255 / mFadeFrames;
    ApplyAlpha(alpha);
}

void LayoutObj::Draw() {
    f32 near = 0.0f;
    f32 far = 1.0f;
    mLayout->CalculateMtx(*mDrawInfo);

    if ((s32)mLayout->GetOriginType() == nw4r::lyt::ORIGINTYPE_CENTER) {
        near = -near;
        far = -far;
    }

    Mtx44 proj;
    C_MTXOrtho(proj, mLayout->GetLayoutRect().top, mLayout->GetLayoutRect().bottom,
               mLayout->GetLayoutRect().left, mLayout->GetLayoutRect().right, near, far);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    GXSetNumChans(1);
    GXSetCullMode(GX_CULL_NONE);
    GXSetZMode(GX_FALSE, GX_NEVER, GX_FALSE);

    for (s32 i = 0; i < mNumItems; i++) {
        GetItem(i)->Draw();
    }
}
