#include "Tween.h"

Tween::Tween() : mValue(0.0f), mTarget(0.0f), mStep(0.0f) {}

void Tween::Update() {
    if (this->mValue > this->mTarget) {
        this->mValue -= this->mStep;
        if (this->mValue < this->mTarget) {
            this->mValue = this->mTarget;
        }
    } else if (this->mValue < this->mTarget) {
        this->mValue += this->mStep;
        if (this->mValue > this->mTarget) {
            this->mValue = this->mTarget;
        }
    }
}
