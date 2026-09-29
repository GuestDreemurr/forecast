#ifndef CHANNEL_TEXT_TAG_PROCESSOR_H
#define CHANNEL_TEXT_TAG_PROCESSOR_H
#include <types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_TagProcessorBase.h>

// Handles the channel's text color tags when drawing layout text
class TextTagProcessor : public nw4r::ut::TagProcessorBase<wchar_t> {
public:
    TextTagProcessor();
    virtual ~TextTagProcessor();

    nw4r::ut::Color mSavedColor; // at 0x4
};

#endif
