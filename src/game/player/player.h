//
// Created by William on 2024-12-15.
//

#ifndef PLAYER_H
#define PLAYER_H


class Camera;

class Player
{
public:
    Player();

    virtual ~Player();

    virtual void update(float deltaTime);

    [[nodiscard]] const Camera* getCamera() const { return camera; }

private:
    Camera* camera{nullptr};
};


#endif //PLAYER_H
