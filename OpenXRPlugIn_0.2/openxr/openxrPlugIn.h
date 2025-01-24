#pragma once


#define XR_USE_GRAPHICS_API_OPENGL
#define XR_KHR_OPENGL_ENABLE_EXTENSION_NAME "XR_KHR_opengl_enable"
#define XR_USE_PLATFORM_WIN32
#define NOMINMAX
#include "Windows.h"
#include "openxr.h"
#include "openxr_platform.h"
#include "core/ecs.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>


//ALWAYS 0 = LEFT, 1 = RIGHT
typedef enum ControllerIndx
{
    LEFT_CONTROLLER_INDX,
    RIGHT_CONTROLLER_INDX
} ControllerIndx;


class OpenxrPlugIn : public bee::System
{
public:

	OpenxrPlugIn();
    ~OpenxrPlugIn();

    //INIT
	bool Init();
	
	void  CreateInstance();
    void  CreateDebugMessenger();
    void  GetInstanceProperties();
    void  GetSystemID();        
    XrPath CreateXrPath(const char* path_string);   // Helper functions from string to path
    std::string FromXrPath(XrPath path);            // and <->
    void  CreateActionSet();
    void  SuggestBindings();
    void  GetViewConfigurationViews();
    void  GetEnvironmentBlendModes();
    void CreateSession();
    void CreateActionPoses();    
    void AttachActionSet();
    void CreateReferenceSpace();
    void CreateSwapchains();
   
    //Update
    void PollEvents();
    void RecordCurrentBindings();
    void PollActions(XrTime predictedTime);

    void GetControllerPose(int controllerIndx, float& pos_x, float& pos_y, float& pos_z, glm::quat& rot);

    

    //Render
    struct RenderLayerInfo
    {
        XrTime predictedDisplayTime;
        std::vector<XrCompositionLayerBaseHeader*> layers;
        XrCompositionLayerProjection layerProjection = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerProjectionView> layerProjectionViews;

        // AR
        std::vector<XrCompositionLayerDepthInfoKHR> layerDepthInfos;
    };

    void RenderXRBeguin();
    bool RenderLayer(RenderLayerInfo& renderLayerInfo);
    void BlitToSwapchain(int eyeIndex, int finalBufferIndx, int finalBufferTextureWidth, int finalBufferTextureHeight);
    void RenderXREnd();


// XR essentials
#pragma region variables

    XrInstance m_xrInstance = {};
    std::vector<const char*> m_activeAPILayers = {};
    std::vector<const char*> m_activeInstanceExtensions = {};
    std::vector<std::string> m_apiLayers = {};
    std::vector<std::string> m_instanceExtensions = {};

    XrDebugUtilsMessengerEXT m_debugUtilsMessenger = {};

    XrFormFactor m_formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId m_systemID = {};
    XrSystemProperties m_systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};

    XrSession m_session = XR_NULL_HANDLE;
    XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;

    XrGraphicsBindingOpenGLWin32KHR graphicsBinding;

#pragma endregion

// Input
#pragma region variables

    XrActionSet m_actionSet;
    // An action for grabbing blocks, and an action to change the color of a block.
    XrAction m_grabCubeAction, m_spawnCubeAction, m_changeColorAction;
    // The realtime states of these actions.
    XrActionStateFloat m_grabState[2] = {{XR_TYPE_ACTION_STATE_FLOAT}, {XR_TYPE_ACTION_STATE_FLOAT}};
    XrActionStateBoolean m_changeColorState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};
    XrActionStateBoolean m_spawnCubeState = {XR_TYPE_ACTION_STATE_BOOLEAN};
    // The haptic output action for grabbing cubes.
    XrAction m_buzzAction;
    // The current haptic output value for each controller.
    float m_buzz[2] = {0, 0};
    // The action for getting the hand or controller position and orientation.
    XrAction m_palmPoseAction;
    // The XrPaths for left and right hand hands or controllers.
    XrPath m_handPaths[2] = {0, 0};
    // The spaces that represents the two hand poses.
    XrSpace m_handPoseSpace[2];
    XrActionStatePose m_handPoseState[2] = {{XR_TYPE_ACTION_STATE_POSE}, {XR_TYPE_ACTION_STATE_POSE}};
    // The current poses obtained from the XrSpaces.
    float m_viewHeightM = 1.5f;
    XrPosef m_handPose[2] = {{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -m_viewHeightM}},
                             {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -m_viewHeightM}}};
    

#pragma endregion

// Input Abstraction
#pragma region variables

    // Left
    XrAction leftTrigger;
    XrActionStateFloat leftTriggerState = {XR_TYPE_ACTION_STATE_FLOAT};

    XrAction leftGrip;
    XrActionStateFloat leftGripState = {XR_TYPE_ACTION_STATE_FLOAT};

    XrAction x_button;    
    XrActionStateBoolean x_buttonState = {XR_TYPE_ACTION_STATE_BOOLEAN}; 

    XrAction y_button;
    XrActionStateBoolean y_buttonState = {XR_TYPE_ACTION_STATE_BOOLEAN};

    XrAction leftThumbstick_click;
    XrActionStateBoolean leftThumbstick_clickState = {XR_TYPE_ACTION_STATE_BOOLEAN};

    XrAction leftJoystick_x;
    XrActionStateFloat leftJoystick_x_State = {XR_TYPE_ACTION_STATE_FLOAT};

    XrAction leftJoystick_y;
    XrActionStateFloat leftJoystick_y_State = {XR_TYPE_ACTION_STATE_FLOAT};

    // Right
    XrAction rightTrigger;
    XrActionStateFloat rightTriggerState = {XR_TYPE_ACTION_STATE_FLOAT};

    XrAction rightGrip;
    XrActionStateFloat rightGripState = {XR_TYPE_ACTION_STATE_FLOAT};

    XrAction a_button;
    XrActionStateBoolean a_buttonState = {XR_TYPE_ACTION_STATE_BOOLEAN};

    XrAction b_button;
    XrActionStateBoolean b_buttonState = {XR_TYPE_ACTION_STATE_BOOLEAN};

    XrAction rightThumbstick_click;
    XrActionStateBoolean rightThumbstick_clickState = {XR_TYPE_ACTION_STATE_BOOLEAN};

    XrAction rightJoystick_x;
    XrActionStateFloat rightJoystick_x_State = {XR_TYPE_ACTION_STATE_FLOAT};

    XrAction rightJoystick_y;
    XrActionStateFloat rightJoystick_y_State = {XR_TYPE_ACTION_STATE_FLOAT};

#pragma endregion


// View
#pragma region variables

    std::vector<XrViewConfigurationType> m_applicationViewConfigurations = {XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                                            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO};
    std::vector<XrViewConfigurationType> m_viewConfigurations;
    XrViewConfigurationType m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
    std::vector<XrViewConfigurationView> m_viewConfigurationViews;

public:
    struct SwapchainInfo
    {
        XrSwapchain swapchain = XR_NULL_HANDLE;
        int64_t swapchainFormat = 0;
        std::vector<void*> imageViews;
    };
    std::vector<SwapchainInfo> m_colorSwapchainInfos = {};
    std::vector<SwapchainInfo> m_depthSwapchainInfos = {};

    std::vector<std::vector<XrSwapchainImageOpenGLKHR>> swapchainImages;

    unsigned int swapchainImageIndx = 0;
    unsigned int eyeIndx = 0;
#pragma endregion

// Layer and blend
#pragma region variables

    std::vector<XrEnvironmentBlendMode> m_applicationEnvironmentBlendModes = {XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
                                                                              XR_ENVIRONMENT_BLEND_MODE_ADDITIVE};
    std::vector<XrEnvironmentBlendMode> m_environmentBlendModes = {};
    XrEnvironmentBlendMode m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;

    XrSpace m_localSpace = XR_NULL_HANDLE;


#pragma endregion



};
