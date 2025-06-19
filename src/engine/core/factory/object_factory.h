//
// Created by William on 2025-06-17.
//

#ifndef OBJECT_FACTORY_H
#define OBJECT_FACTORY_H

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <type_traits>

namespace will_engine::game
{
template<typename T>
concept HasGetStaticType = requires(T)
{
    { T::getStaticType() } -> std::convertible_to<std::string_view>;
};

template<typename BaseType>
class ObjectFactory
{
protected:
    ObjectFactory() = default;

public:
    using Creator = std::function<std::unique_ptr<BaseType>(const std::string& name)>;

    // If singleton access is required, define in the individual sub-classes
    // static ObjectFactory& getInstance()
    // {
    //     static ObjectFactory instance;
    //     return instance;
    // }

    const std::unordered_map<std::string_view, Creator>& getManuallyCreatableCreators() const
    {
        return canManuallyCreateCreators;
    }

    template<typename T>
    void registerType(bool canManuallyCreate) requires HasGetStaticType<T>
    {
        static_assert(std::is_base_of_v<BaseType, T>, "T must inherit from BaseType");

        allCreators[T::getStaticType()] = [](const std::string& name) {
            return std::make_unique<T>(name);
        };

        if (canManuallyCreate) {
            canManuallyCreateCreators[T::getStaticType()] = [](const std::string& name) {
                return std::make_unique<T>(name);
            };
        }
    }

    std::unique_ptr<BaseType> create(const std::string_view& type, const std::string& name) const
    {
        const auto it = allCreators.find(type);
        if (it != allCreators.end()) {
            return it->second(name);
        }
        return nullptr;
    }

    template<typename U>
    std::unique_ptr<U> create(const std::string& name) const requires HasGetStaticType<U>
    {
        static_assert(std::is_base_of_v<BaseType, U>, "U must inherit from BaseType");

        const auto it = allCreators.find(U::getStaticType());
        if (it != allCreators.end()) {
            return it->second(name);
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string_view, Creator> allCreators;
    std::unordered_map<std::string_view, Creator> canManuallyCreateCreators;
};
} // will_engine

#endif //OBJECT_FACTORY_H
