#pragma once
#include "core/ecs.hpp"
#include "core/engine.hpp"
#include "core/transform.hpp"
#include "openxrPlugIn.h"

namespace bee
{

struct XRController
{
    XRController() {}
    XRController(Entity entity, ControllerIndx controllerIndx)
    { 
        m_entity = entity;
        m_controllerIndex = controllerIndx;
    }

    ControllerIndx GetControllerIndx() { return m_controllerIndex; }
    float GetTriggerValue() { return m_trigger; }
    float GetGripValue() { return m_Grip; }
    float Get_a_x_Value() { return m_a_x_button; }
    float Get_b_y_Value() { return m_b_y_button; }
    float GetJoystick_click_Value() { return m_Thumbstick_click; }
    float GetJoystick_x_Value() { return m_Joystick_x; }
    float GetJoystick_y_Value() { return m_Joystick_y; }

    void UpdateInput(OpenxrPlugIn& openXRPlugIn, ControllerIndx ci)
    {       
        openXRPlugIn.GetControllerPose(ci, m_position.x, m_position.y, m_position.z, m_orientation);        
        
        if (ci == ControllerIndx::LEFT_CONTROLLER_INDX)
        {
            m_trigger = openXRPlugIn.leftTriggerState.currentState;
            m_Grip = openXRPlugIn.leftGripState.currentState;
            m_a_x_button = openXRPlugIn.x_buttonState.currentState;
            m_b_y_button = openXRPlugIn.y_buttonState.currentState;
            m_Thumbstick_click = openXRPlugIn.leftThumbstick_clickState.currentState;
            m_Joystick_x = openXRPlugIn.leftJoystick_x_State.currentState;
            m_Joystick_y = openXRPlugIn.leftJoystick_y_State.currentState;
        }
        else
        {
            m_trigger = openXRPlugIn.rightTriggerState.currentState;
            m_Grip = openXRPlugIn.rightGripState.currentState;
            m_a_x_button = openXRPlugIn.a_buttonState.currentState;
            m_b_y_button = openXRPlugIn.b_buttonState.currentState;
            m_Thumbstick_click = openXRPlugIn.rightThumbstick_clickState.currentState;
            m_Joystick_x = openXRPlugIn.rightJoystick_x_State.currentState;
            m_Joystick_y = openXRPlugIn.rightJoystick_y_State.currentState;
        }
    }

    private:
    Entity m_entity;
    ControllerIndx m_controllerIndex;

    glm::vec3 m_position;
    glm::quat m_orientation;

    float m_trigger = 0;     
    float m_Grip = 0;
    bool m_a_x_button = false;
    bool m_b_y_button = false;
    bool m_Thumbstick_click = false;
    float m_Joystick_x = 0;
    float m_Joystick_y = 0;
    
};

struct SphereSensor
{};



class XRComponentsSystem : public bee::System
{
public:

    XRComponentsSystem(){};
    ~XRComponentsSystem(){};

    void CreateXRRig( Entity& xrRigEntityOut, Entity& xrCameraEntityOut, Entity& l_xrControllerEntityOut,  Entity& r_xrControllerEntityOut, std::string transformName = "XRrig")
    {
        auto xrRigEntity = CreateXRRigParent();
        auto xrCameraEntity = CreateXRCamera(xrRigEntity);
        auto l_xrControllerEntity = CreateXRController(xrRigEntity, ControllerIndx::LEFT_CONTROLLER_INDX);
        auto r_xrControllerEntity = CreateXRController(xrRigEntity, ControllerIndx::RIGHT_CONTROLLER_INDX);

        xrRigEntityOut =  xrRigEntity;
        xrCameraEntityOut = xrCameraEntity;
        l_xrControllerEntityOut = l_xrControllerEntity;
        r_xrControllerEntityOut = r_xrControllerEntity;
    }

    Entity CreateXRRigParent(std::string transformName = "XRrig")
    {
        auto xrRigEntity = Engine.ECS().CreateEntity();
        auto& transform_xrRig =
            Engine.ECS().CreateComponent<Transform>(xrRigEntity, glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), glm::quat(1, 0, 0, 0));
        transform_xrRig.Name = transformName;

        return xrRigEntity;
    }
    Entity CreateXRCamera(Entity xrRigEntity, std::string transformName = "CameraVR")
    {
        // Camera
        auto camera_entity = Engine.ECS().CreateEntity();
        auto& camera_transform = Engine.ECS().CreateComponent<Transform>(camera_entity);
        camera_transform.Name = transformName;
        glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -2), glm::vec3(0, 1, 0));
        camera_transform.SetFromMatrix(view);
        camera_transform.SetTranslation(glm::vec3(0, 0, 0));
        camera_transform.SetParent(xrRigEntity);
        Engine.ECS().CreateComponent<Camera>(camera_entity);

        return camera_entity;
    }
    Entity CreateXRController(Entity xrRigEntity, ControllerIndx controllerIndx ,std::string transformName = "Hand")
    {
        // Left hand
        auto hand_entity = Engine.ECS().CreateEntity();
        auto& hand_transform = Engine.ECS().CreateComponent<Transform>(hand_entity, glm::vec3(0), glm::vec3(1), glm::quat(1, 0, 0, 0));
        hand_transform.SetParent(xrRigEntity);

        std::string n = (controllerIndx == LEFT_CONTROLLER_INDX) ? "l" : "r";
        hand_transform.Name = n + transformName;

        auto l_xrController = Engine.ECS().CreateComponent<XRController>(hand_entity, hand_entity, controllerIndx);

        return hand_entity;
    }

    void Update(float dt)
    {
        (void)dt;

        auto openXRPlugIn = bee::Engine.ECS().GetSystem<OpenxrPlugIn>();

        //Update controller
        for (auto [entity, transform, xrController] : bee::Engine.ECS().Registry.view<bee::Transform, bee::XRController>().each())
        {
            glm::vec3 updatedPos;
            glm::quat updatedRot;
            openXRPlugIn.GetControllerPose(xrController.GetControllerIndx(), updatedPos.x, updatedPos.y, updatedPos.z, updatedRot);

            transform.SetRotation(updatedRot);
            transform.SetTranslation(updatedPos);

            //Input
            xrController.UpdateInput(openXRPlugIn, xrController.GetControllerIndx());
        }
    }
    
};

}  // namespace bee
