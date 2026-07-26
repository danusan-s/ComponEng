#include "aim_system.hpp"

#include <GLFW/glfw3.h>
#include <cmath>
#include <limits>
#include <vector>

#include <imgui/imgui.h>

#include "componeng/components/collider_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/debug_ui.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/resources/input_state.hpp"
#include "componeng/resources/main_camera.hpp"

namespace aim_train {

using namespace componeng;

static void drawCrosshair() {
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    float size = 12.0f;
    float gap = 4.0f;
    float thickness = 2.0f;
    ImU32 color = IM_COL32(255, 255, 255, 200);

    dl->AddLine(ImVec2(c.x - size, c.y), ImVec2(c.x - gap, c.y), color, thickness);
    dl->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + size, c.y), color, thickness);
    dl->AddLine(ImVec2(c.x, c.y - size), ImVec2(c.x, c.y - gap), color, thickness);
    dl->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + size), color, thickness);
}

core::Vec3 cameraFront(core::Vec3 rotation) {
    float pitch = core::radians(rotation.x);
    float yaw   = core::radians(rotation.y);
    core::Vec3 front;
    front.x = cos(yaw) * cos(pitch);
    front.y = sin(pitch);
    front.z = sin(yaw) * cos(pitch);
    return core::normalize(front);
}

bool raySphereIntersect(core::Vec3 origin, core::Vec3 dir,
                        core::Vec3 center, float radius, float &t) {
    core::Vec3 oc = origin - center;
    float a = core::dot(dir, dir);
    float b = 2.0f * core::dot(oc, dir);
    float c = core::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f)
        return false;
    float sqrtD = std::sqrt(discriminant);
    float t1 = (-b - sqrtD) / (2.0f * a);
    float t2 = (-b + sqrtD) / (2.0f * a);
    t = (t1 > 0.0f) ? t1 : t2;
    return t > 0.0f;
}

void AimSystem::onUpdate(const ecs::SystemState &state) {
    auto &input = state.world->get_resource<resources::InputState>();
    auto &actions = state.world->get_resource<resources::ActionState>();
    auto &mainCam = state.world->get_resource<resources::MainCamera>();
    auto &camTransform =
        state.world->getComponent<components::TransformComponent>(mainCam.entity);

    camTransform.rotation.y += input.getMouseDeltaX() * m_sensitivity;
    camTransform.rotation.x -= input.getMouseDeltaY() * m_sensitivity;

    constexpr float PITCH_LIMIT = 89.0f;
    if (camTransform.rotation.x > PITCH_LIMIT)
        camTransform.rotation.x = PITCH_LIMIT;
    if (camTransform.rotation.x < -PITCH_LIMIT)
        camTransform.rotation.x = -PITCH_LIMIT;

    float speed = 100.0f * state.deltaTime;
    if (actions.down(resources::Action::Sprint))
        speed *= 2.0f;

    core::Vec3 front = cameraFront(camTransform.rotation);
    core::Vec3 right = core::normalize(
        core::cross(front, core::Vec3(0.0f, 1.0f, 0.0f)));

    if (actions.down(resources::Action::MoveForward))
        camTransform.position += front * speed;
    if (actions.down(resources::Action::MoveBackward))
        camTransform.position -= front * speed;
    if (actions.down(resources::Action::MoveLeft))
        camTransform.position -= right * speed;
    if (actions.down(resources::Action::MoveRight))
        camTransform.position += right * speed;

    drawCrosshair();
    core::DebugUI::addValue("Score", static_cast<float>(m_score));
    ImGui::InputFloat("Sensitivity", &m_sensitivity, 0.01f, 0.1f, "%.3f");

    if (!input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT))
        return;

    core::Vec3 rayOrigin = camTransform.position;
    core::Vec3 rayDir    = front;

    struct Target {
        ecs::EntityID id;
        core::Vec3 center;
        float radius;
    };
    std::vector<Target> targets;

    auto q = state.world->query<components::TransformComponent,
                                 components::ColliderComponent>();
    q.eachWithEntity(
        [&](ecs::EntityID entity, components::TransformComponent &t,
            components::ColliderComponent &c) {
            if (c.type == components::ColliderType::Sphere) {
                targets.push_back({entity, t.position, t.scale.x});
            }
        });

    float closestT = std::numeric_limits<float>::max();
    ecs::EntityID hitEntity = ecs::INVALID_ENTITY;

    for (auto &target : targets) {
        float t;
        if (raySphereIntersect(rayOrigin, rayDir, target.center,
                               target.radius, t) &&
            t < closestT) {
            closestT = t;
            hitEntity = target.id;
        }
    }

    if (hitEntity != ecs::INVALID_ENTITY) {
        state.world->destroyEntity(hitEntity);
        ++m_score;
    }
}

}
