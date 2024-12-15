//
// Created by William on 2024-12-15.
//

#ifndef PLAYER_H
#define PLAYER_H


class Camera;

class PlayerCharacter
{
public:
    PlayerCharacter();

    virtual ~PlayerCharacter();

    virtual void update(float deltaTime);

    [[nodiscard]] const Camera* getCamera() const { return camera; }

private:
    void AddForceToObject() const;

protected:
    Camera* camera{nullptr};
};


#endif //PLAYER_H
