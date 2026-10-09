#ifndef CHANNEL_LAYOUTOBJ_H
#define CHANNEL_LAYOUTOBJ_H
#include <types.h>
#include <nw4r/math/math_types.h>
#include <revolution/GX.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_Rect.h>

namespace nw4r {
namespace lyt {
class Pane;
class Layout;
class DrawInfo;
class ArcResourceAccessor;
}
}

class TextTagProcessor;

// One entry of LayoutObj::mItems (size 0x98), built from a child of the layout's root pane.
// Its parts are the panes named after it plus a letter, e.g. "<name>T" for the text.
struct LayoutObjItem {
    LayoutObjItem(nw4r::lyt::Pane* pane, nw4r::lyt::DrawInfo* drawInfo, TextTagProcessor* tagProcessor,
                  s32 arg);
    nw4r::lyt::Pane* GetChildPane(s32 index);
    void MarkTree();
    void Update();
    void Draw();

    nw4r::lyt::Pane* mPane;         // at 0x0, root pane of the item
    nw4r::lyt::DrawInfo* mDrawInfo; // at 0x4
    nw4r::lyt::Pane* mPaneB;        // at 0x8, "<name>B", or mPane if there is none
    nw4r::lyt::Pane* mPaneI;        // at 0xC, "<name>I" child for the current language, else "<name>I"
    nw4r::lyt::Pane* mPaneT;        // at 0x10, "<name>T" child for the current language
    nw4r::lyt::Pane* mPaneF0;       // at 0x14, "<name>F0"
    nw4r::lyt::Pane* mPaneF1;       // at 0x18, "<name>F1"
    nw4r::lyt::Pane* mPaneM;        // at 0x1C, "<name>M" child for the current language
    nw4r::ut::Rect mRect;           // at 0x20, bounds of "<name>R" (or mPaneB) at the item's position
    nw4r::math::VEC3 mOrigTrans;    // at 0x30
    u8 unk3C;                       // at 0x3C, set by an 'F' tag in the user data
    u8 unk3D[0x40 - 0x3D];          // at 0x3D
    s32 mType;                      // at 0x40, set by a "C<digit>" tag in the user data
    u8 unk44;                       // at 0x44
    u8 unk45;                       // at 0x45
    u8 unk46;                       // at 0x46
    u8 unk47;                       // at 0x47
    u8 unk48;                       // at 0x48, set: ignore the slide offset
    u8 unk49;                       // at 0x49
    u8 unk4A;                       // at 0x4A, set by a 'D' tag in the user data
    u8 unk4B;                       // at 0x4B
    LayoutObjItem* unk4C;           // at 0x4C
    LayoutObjItem* unk50;           // at 0x50
    f32 mSlideOffset;               // at 0x54
    s32 mAlpha;                     // at 0x58
    s32 unk5C;                      // at 0x5C
    u8 unk60;                       // at 0x60
    u8 unk61[0x64 - 0x61];          // at 0x61
    s32 unk64;                      // at 0x64
    u8 unk68;                       // at 0x68
    u8 unk69[0x6C - 0x69];          // at 0x69
    s32 unk6C;                      // at 0x6C
    s32 unk70;                      // at 0x70, index of the mPaneT child to show
    u8 unk74;                       // at 0x74
    u8 unk75[0x78 - 0x75];          // at 0x75
    nw4r::ut::Color mTextColor;     // at 0x78
    s32 unk7C;                      // at 0x7C, index of the mPaneM child to show
    s32 unk80;                      // at 0x80, index of that child's child to show
    void* unk84;                    // at 0x84
    void* unk88;                    // at 0x88
    void* unk8C;                    // at 0x8C
    void* unk90;                    // at 0x90
    void* unk94;                    // at 0x94
};

// Wraps an nw4r::lyt layout loaded from an archive (0x12C bytes)
class LayoutObj {
public:
    LayoutObj(const void* archive, const char* layoutName, s32 arg);
    ~LayoutObj();

    // Some functions read mItems through this and some directly: the two compile differently,
    // and each function uses the form that matches the original.
    inline LayoutObjItem* GetItem(s32 i) { return mItems[i]; }
    inline void ApplyAlpha(s32 alpha);
    inline void InitState();
    void Reset();
    void Calc();
    void Draw();

    nw4r::lyt::ArcResourceAccessor* mResAccessor; // at 0x0
    nw4r::lyt::Layout* mLayout;      // at 0x4
    nw4r::lyt::DrawInfo* mDrawInfo;  // at 0x8
    LayoutObjItem* mItems[0x40];     // at 0xC, only mNumItems entries are used
    s32 mNumItems;                   // at 0x10C
    TextTagProcessor* mTagProcessor; // at 0x110
    u8 mSlidingOut;                  // at 0x114
    u8 unk115[0x118 - 0x115];        // at 0x115
    s32 mSlideFrames;                // at 0x118
    s32 mSlideFrame;                 // at 0x11C
    u8 mFadingOut;                   // at 0x120
    u8 unk121[0x124 - 0x121];        // at 0x121
    s32 mFadeFrames;                 // at 0x124
    s32 mFadeFrame;                  // at 0x128
};

#endif
