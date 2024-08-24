//
// Created by William on 2024-08-24.
//

#ifndef CAMERA_H
#define CAMERA_H


class camera
{
protected:
    // vert rot
    float pitch{0.f};
    // horizontal rot
    float yaw{0.f};

    void processSdlEvent();

    virtual void update();
};


#endif //CAMERA_H
