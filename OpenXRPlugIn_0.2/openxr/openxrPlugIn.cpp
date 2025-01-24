#include "openxrPlugIn.h"
#include "core/engine.hpp"
#include "core/device.hpp"
#include <cstdio>
#include <iostream>
#include "DebugOutput.h"

#include "OpenXRDebugUtils.h"


#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "renderXR_gl.h"

#include "core/transform.hpp"
#include "xr_linear_algebra.h"



OpenxrPlugIn::OpenxrPlugIn()
{}

OpenxrPlugIn::~OpenxrPlugIn() 
{}



//INIT

bool OpenxrPlugIn::Init() 
{
    CreateInstance();
    CreateDebugMessenger();

    GetInstanceProperties();
    GetSystemID();

    CreateActionSet();
    SuggestBindings();

    GetViewConfigurationViews();
    GetEnvironmentBlendModes();

    CreateSession();
    CreateActionPoses();    
    AttachActionSet();
    CreateReferenceSpace();

    CreateSwapchains();

	return true; 
}

void OpenxrPlugIn::CreateInstance() 
{
    XrApplicationInfo AI;
    strncpy(AI.applicationName, "OpenXR plug-in", XR_MAX_APPLICATION_NAME_SIZE);  // EDIT GRAPHICS
    AI.applicationVersion = 1;
    strncpy(AI.engineName, "Bee Engine", XR_MAX_ENGINE_NAME_SIZE);
    AI.engineVersion = 1;
    AI.apiVersion = XR_CURRENT_API_VERSION;  // EDIT GRAPHICS

    m_instanceExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    // Ensure m_apiType is already defined when we call this line.
    m_instanceExtensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
    // AR
    m_instanceExtensions.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
    // m_instanceExtensions.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);

    // Get all the API Layers from the OpenXR runtime.
    uint32_t apiLayerCount = 0;
    std::vector<XrApiLayerProperties> apiLayerProperties;
    OPENXR_CHECK(xrEnumerateApiLayerProperties(0, &apiLayerCount, nullptr), "Failed to enumerate ApiLayerProperties.");
    apiLayerProperties.resize(apiLayerCount, {XR_TYPE_API_LAYER_PROPERTIES});
    OPENXR_CHECK(xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount, apiLayerProperties.data()),
                 "Failed to enumerate ApiLayerProperties.");

    // Check the requested API layers against the ones from the OpenXR. If found add it to the Active API Layers.
    for (auto& requestLayer : m_apiLayers)
    {
        for (auto& layerProperty : apiLayerProperties)
        {
            // strcmp returns 0 if the strings match.
            if (strcmp(requestLayer.c_str(), layerProperty.layerName) != 0)
            {
                continue;
            }
            else
            {
                m_activeAPILayers.push_back(requestLayer.c_str());
                break;
            }
        }
    }

    // Get all the Instance Extensions from the OpenXR instance.
    uint32_t extensionCount = 0;
    std::vector<XrExtensionProperties> extensionProperties;
    OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr),
                 "Failed to enumerate InstanceExtensionProperties.");
    extensionProperties.resize(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
    OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()),
                 "Failed to enumerate InstanceExtensionProperties.");

    // Check the requested Instance Extensions against the ones from the OpenXR runtime.
    // If an extension is found add it to Active Instance Extensions.
    // Log error if the Instance Extension is not found.
    for (auto& requestedInstanceExtension : m_instanceExtensions)
    {
        bool found = false;
        for (auto& extensionProperty : extensionProperties)
        {
            // strcmp returns 0 if the strings match.
            if (strcmp(requestedInstanceExtension.c_str(), extensionProperty.extensionName) != 0)
            {
                continue;
            }
            else
            {
                m_activeInstanceExtensions.push_back(requestedInstanceExtension.c_str());
                found = true;
                break;
            }
        }
        if (!found)
        {
            XR_TUT_LOG_ERROR("Failed to find OpenXR instance extension: " << requestedInstanceExtension);
        }
    }

    XrInstanceCreateInfo instanceCI{XR_TYPE_INSTANCE_CREATE_INFO};
    instanceCI.createFlags = 0;
    instanceCI.applicationInfo = AI;
    instanceCI.enabledApiLayerCount = static_cast<uint32_t>(m_activeAPILayers.size());
    instanceCI.enabledApiLayerNames = m_activeAPILayers.data();
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(m_activeInstanceExtensions.size());
    instanceCI.enabledExtensionNames = m_activeInstanceExtensions.data();
    OPENXR_CHECK(xrCreateInstance(&instanceCI, &m_xrInstance), "Failed to create Instance.");
}

void OpenxrPlugIn::CreateDebugMessenger() 
{
    // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before creating an XrDebugUtilsMessengerEXT.
    if (IsStringInVector(m_activeInstanceExtensions, XR_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        m_debugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance);  // From OpenXRDebugUtils.h.
    }
}

void OpenxrPlugIn::GetInstanceProperties() 
{
    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
    OPENXR_CHECK(xrGetInstanceProperties(m_xrInstance, &instanceProperties), "Failed to get InstanceProperties.");

    XR_TUT_LOG("OpenXR Runtime: " << instanceProperties.runtimeName << " - "
                                  << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "."
                                  << XR_VERSION_MINOR(instanceProperties.runtimeVersion) << "."
                                  << XR_VERSION_PATCH(instanceProperties.runtimeVersion));
}

void OpenxrPlugIn::GetSystemID() 
{
    // Get the XrSystemId from the instance and the supplied XrFormFactor.
    XrSystemGetInfo systemGI{XR_TYPE_SYSTEM_GET_INFO};
    systemGI.formFactor = m_formFactor;
    OPENXR_CHECK(xrGetSystem(m_xrInstance, &systemGI, &m_systemID), "Failed to get SystemID.");

    // Get the System's properties for some general information about the hardware and the vendor.
    OPENXR_CHECK(xrGetSystemProperties(m_xrInstance, m_systemID, &m_systemProperties), "Failed to get SystemProperties.");
}

XrPath OpenxrPlugIn::CreateXrPath(const char* path_string)    
{
    XrPath xrPath;
    OPENXR_CHECK(xrStringToPath(m_xrInstance, path_string, &xrPath), "Failed to create XrPath from string.");
    return xrPath;
}

std::string OpenxrPlugIn::FromXrPath(XrPath path)
{
    uint32_t strl;
    char text[XR_MAX_PATH_LENGTH];
    XrResult res;
    res = xrPathToString(m_xrInstance, path, XR_MAX_PATH_LENGTH, &strl, text);
    std::string str;
    if (res == XR_SUCCESS)
    {
        str = text;
    }
    else
    {
        OPENXR_CHECK(res, "Failed to retrieve path.");
    }
    return str;
}
// EDIT FOR THE APP;
void OpenxrPlugIn::CreateActionSet()
{
    XrActionSetCreateInfo actionSetCI{XR_TYPE_ACTION_SET_CREATE_INFO};
    // The internal name the runtime uses for this Action Set.
    strncpy(actionSetCI.actionSetName, "openxr-tutorial-actionset", XR_MAX_ACTION_SET_NAME_SIZE);
    // Localized names are required so there is a human-readable action name to show the user if they are rebinding Actions in
    // an options screen.
    strncpy(actionSetCI.localizedActionSetName, "OpenXR Tutorial ActionSet", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
    // Set a priority: this comes into play when we have multiple Action Sets, and determines which Action takes priority in
    // binding to a specific input.
    actionSetCI.priority = 0;

    OPENXR_CHECK(xrCreateActionSet(m_xrInstance, &actionSetCI, &m_actionSet), "Failed to create ActionSet.");

    auto CreateAction = [this](XrAction& xrAction,
                               const char* name,
                               XrActionType xrActionType,
                               std::vector<const char*> subaction_paths = {}) -> void
    {
        XrActionCreateInfo actionCI{XR_TYPE_ACTION_CREATE_INFO};
        // The type of action: float input, pose, haptic output etc.
        actionCI.actionType = xrActionType;
        // Subaction paths, e.g. left and right hand. To distinguish the same action performed on different devices.
        std::vector<XrPath> subaction_xrpaths;
        for (auto p : subaction_paths)
        {
            subaction_xrpaths.push_back(CreateXrPath(p));
        }
        actionCI.countSubactionPaths = (uint32_t)subaction_xrpaths.size();
        actionCI.subactionPaths = subaction_xrpaths.data();
        // The internal name the runtime uses for this Action.
        strncpy(actionCI.actionName, name, XR_MAX_ACTION_NAME_SIZE);
        // Localized names are required so there is a human-readable action name to show the user if they are rebinding the
        // Action in an options screen.
        strncpy(actionCI.localizedActionName, name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
        OPENXR_CHECK(xrCreateAction(m_actionSet, &actionCI, &xrAction), "Failed to create Action.");
    };

    // An Action for grabbing cubes.
    CreateAction(m_grabCubeAction, "grab-cube", XR_ACTION_TYPE_FLOAT_INPUT, {"/user/hand/left", "/user/hand/right"});
    CreateAction(m_spawnCubeAction, "spawn-cube", XR_ACTION_TYPE_BOOLEAN_INPUT);
    CreateAction(m_changeColorAction, "change-color", XR_ACTION_TYPE_BOOLEAN_INPUT, {"/user/hand/left", "/user/hand/right"});
    // An Action for the position of the palm of the user's hand - appropriate for the location of a grabbing Actions.
    CreateAction(m_palmPoseAction, "palm-pose", XR_ACTION_TYPE_POSE_INPUT, {"/user/hand/left", "/user/hand/right"});
    // An Action for a vibration output on one or other hand.
    CreateAction(m_buzzAction, "buzz", XR_ACTION_TYPE_VIBRATION_OUTPUT, {"/user/hand/left", "/user/hand/right"});

    //INPUT ABSTRACTION
    
    //Left
    CreateAction(leftTrigger, "left-trigger", XR_ACTION_TYPE_FLOAT_INPUT);
    CreateAction(leftGrip, "left-grip", XR_ACTION_TYPE_FLOAT_INPUT);
    CreateAction(x_button, "x", XR_ACTION_TYPE_BOOLEAN_INPUT);
    CreateAction(y_button, "y", XR_ACTION_TYPE_BOOLEAN_INPUT);
    CreateAction(leftThumbstick_click, "leftstick-click", XR_ACTION_TYPE_BOOLEAN_INPUT);
    CreateAction(leftJoystick_x, "left-joystick-x", XR_ACTION_TYPE_FLOAT_INPUT);
    CreateAction(leftJoystick_y, "left-joystick-y", XR_ACTION_TYPE_FLOAT_INPUT);

    // Right
    CreateAction(rightTrigger, "right-trigger", XR_ACTION_TYPE_FLOAT_INPUT);
    CreateAction(rightGrip, "right-grip", XR_ACTION_TYPE_FLOAT_INPUT);
    CreateAction(a_button, "a-button", XR_ACTION_TYPE_BOOLEAN_INPUT);
    CreateAction(b_button, "b-button", XR_ACTION_TYPE_BOOLEAN_INPUT);
    CreateAction(rightThumbstick_click, "right-stick-click", XR_ACTION_TYPE_BOOLEAN_INPUT);
    CreateAction(rightJoystick_x, "right-joystick-x", XR_ACTION_TYPE_FLOAT_INPUT);
    CreateAction(rightJoystick_y, "right-joystick-y", XR_ACTION_TYPE_FLOAT_INPUT);


    // For later convenience we create the XrPaths for the subaction path names.
    m_handPaths[0] = CreateXrPath("/user/hand/left");
    m_handPaths[1] = CreateXrPath("/user/hand/right");
} 

void OpenxrPlugIn::SuggestBindings() 
{
    auto SuggestBindings = [this](const char* profile_path, std::vector<XrActionSuggestedBinding> bindings) -> bool
    {
        // The application can call xrSuggestInteractionProfileBindings once per interaction profile that it supports.
        XrInteractionProfileSuggestedBinding interactionProfileSuggestedBinding{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        interactionProfileSuggestedBinding.interactionProfile = CreateXrPath(profile_path);
        interactionProfileSuggestedBinding.suggestedBindings = bindings.data();
        interactionProfileSuggestedBinding.countSuggestedBindings = (uint32_t)bindings.size();
        if (xrSuggestInteractionProfileBindings(m_xrInstance, &interactionProfileSuggestedBinding) == XrResult::XR_SUCCESS)
            return true;
        XR_TUT_LOG("Failed to suggest bindings with " << profile_path);
        return false;
    };

    bool any_ok = false;
    // Each Action here has two paths, one for each SubAction path.
    /*any_ok |= SuggestBindings("/interaction_profiles/khr/simple_controller",
                              {{m_changeColorAction, CreateXrPath("/user/hand/left/input/select/click")},
                               {m_grabCubeAction, CreateXrPath("/user/hand/right/input/select/click")},
                               {m_spawnCubeAction, CreateXrPath("/user/hand/right/input/menu/click")},
                               {m_palmPoseAction, CreateXrPath("/user/hand/left/input/grip/pose")},
                               {m_palmPoseAction, CreateXrPath("/user/hand/right/input/grip/pose")},
                               {m_buzzAction, CreateXrPath("/user/hand/left/output/haptic")},
                               {m_buzzAction, CreateXrPath("/user/hand/right/output/haptic")}});*/

    //Oculus specific
    // Each Action here has two paths, one for each SubAction path.
     any_ok |= SuggestBindings("/interaction_profiles/oculus/touch_controller", 
    { 
        //Pose and vibration 0 = left, 1 = right
        {m_palmPoseAction,      CreateXrPath("/user/hand/left/input/grip/pose")},
        {m_palmPoseAction,      CreateXrPath("/user/hand/right/input/grip/pose")},
        {m_buzzAction,          CreateXrPath("/user/hand/left/output/haptic")},
        {m_buzzAction,          CreateXrPath("/user/hand/right/output/haptic")},
        
        //left
        {leftTrigger,           CreateXrPath("/user/hand/left/input/trigger/value")},
        {leftGrip,              CreateXrPath("/user/hand/left/input/squeeze/value")},
        {x_button,              CreateXrPath("/user/hand/left/input/x/click")},
        {y_button,              CreateXrPath("/user/hand/left/input/y/click")},
        {leftThumbstick_click,  CreateXrPath("/user/hand/left/input/thumbstick/click")},
        {leftJoystick_x,        CreateXrPath("/user/hand/left/input/thumbstick/x")},
        {leftJoystick_y,        CreateXrPath("/user/hand/left/input/thumbstick/y")},

        // Right
        {rightTrigger,          CreateXrPath("/user/hand/right/input/trigger/value")},
        {rightGrip,             CreateXrPath("/user/hand/right/input/squeeze/value")},
        {a_button,              CreateXrPath("/user/hand/right/input/a/click")},
        {b_button,              CreateXrPath("/user/hand/right/input/b/click")},
        {rightThumbstick_click, CreateXrPath("/user/hand/right/input/thumbstick/click")},
        {rightJoystick_x,       CreateXrPath("/user/hand/right/input/thumbstick/x")},
        {rightJoystick_y,       CreateXrPath("/user/hand/right/input/thumbstick/y")}
    });

    if (!any_ok)
    {
        DEBUG_BREAK;
    }
}

void OpenxrPlugIn::GetViewConfigurationViews() 
{
    // Gets the View Configuration Types. The first call gets the count of the array that will be returned. The next call fills
    // out the array.
    uint32_t viewConfigurationCount = 0;
    OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_systemID, 0, &viewConfigurationCount, nullptr),
                 "Failed to enumerate View Configurations.");
    m_viewConfigurations.resize(viewConfigurationCount);
    OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance,
                                               m_systemID,
                                               viewConfigurationCount,
                                               &viewConfigurationCount,
                                               m_viewConfigurations.data()),
                 "Failed to enumerate View Configurations.");

    // Pick the first application supported View Configuration Type con supported by the hardware.
    for (const XrViewConfigurationType& viewConfiguration : m_applicationViewConfigurations)
    {
        if (std::find(m_viewConfigurations.begin(), m_viewConfigurations.end(), viewConfiguration) !=
            m_viewConfigurations.end())
        {
            m_viewConfiguration = viewConfiguration;
            break;
        }
    }
    if (m_viewConfiguration == XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM)
    {
        std::cerr << "Failed to find a view configuration type. Defaulting to XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO."
                  << std::endl;
        m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    }

    // Gets the View Configuration Views. The first call gets the count of the array that will be returned. The next call fills
    // out the array.
    uint32_t viewConfigurationViewCount = 0;
    OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance,
                                                   m_systemID,
                                                   m_viewConfiguration,
                                                   0,
                                                   &viewConfigurationViewCount,
                                                   nullptr),
                 "Failed to enumerate ViewConfiguration Views.");
    m_viewConfigurationViews.resize(viewConfigurationViewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance,
                                                   m_systemID,
                                                   m_viewConfiguration,
                                                   viewConfigurationViewCount,
                                                   &viewConfigurationViewCount,
                                                   m_viewConfigurationViews.data()),
                 "Failed to enumerate ViewConfiguration Views.");
}

void OpenxrPlugIn::GetEnvironmentBlendModes() 
{
    // Retrieves the available blend modes. The first call gets the count of the array that will be returned. The next call
    // fills out the array.
    uint32_t environmentBlendModeCount = 0;
    OPENXR_CHECK(
        xrEnumerateEnvironmentBlendModes(m_xrInstance, m_systemID, m_viewConfiguration, 0, &environmentBlendModeCount, nullptr),
        "Failed to enumerate EnvironmentBlend Modes.");
    m_environmentBlendModes.resize(environmentBlendModeCount);
    OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance,
                                                  m_systemID,
                                                  m_viewConfiguration,
                                                  environmentBlendModeCount,
                                                  &environmentBlendModeCount,
                                                  m_environmentBlendModes.data()),
                 "Failed to enumerate EnvironmentBlend Modes.");

    // Pick the first application supported blend mode supported by the hardware.
    for (const XrEnvironmentBlendMode& environmentBlendMode : m_applicationEnvironmentBlendModes)
    {
        // AR
        if (std::find(m_environmentBlendModes.begin(), m_environmentBlendModes.end(), environmentBlendMode) !=
            m_environmentBlendModes.end())
        {
            if (environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND ||
                environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_ADDITIVE)
            {
                m_environmentBlendMode = environmentBlendMode;
                break;
            }
        }
    }
    if (m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM)
    {
        XR_TUT_LOG_ERROR("Failed to find a compatible blend mode. Defaulting to XR_ENVIRONMENT_BLEND_MODE_OPAQUE.");
        m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    }
}

void OpenxrPlugIn::CreateSession() 
{
    // Query OpenGL graphics requirements
    PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = nullptr;
    OPENXR_CHECK(xrGetInstanceProcAddr(m_xrInstance,
                                       "xrGetOpenGLGraphicsRequirementsKHR",
                                       (PFN_xrVoidFunction*)&pfnGetOpenGLGraphicsRequirementsKHR),
                 "Failed to get OpenGL graphics requirements function pointer");

    XrGraphicsRequirementsOpenGLKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    OPENXR_CHECK(pfnGetOpenGLGraphicsRequirementsKHR(m_xrInstance, m_systemID, &graphicsRequirements),
                 "Failed to get OpenGL graphics requirements");
      
    //////////////////////////////////////////////////////////////////////////////////////////////
    
    GLFWwindow* window = nullptr;
    window = bee::Engine.Device().GetWindow();   
    HWND hwnd = glfwGetWin32Window(window);  // Get the Win32 window handle
    HDC hdc = GetDC(hwnd);                                              // Get the window 
    
    HGLRC hglrc = wglGetCurrentContext();    

    // Get the OpenGl context

    // Fill the graphics binding structure
    graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
    graphicsBinding.next = nullptr;
    graphicsBinding.hDC = hdc;      // Valid HDC handle
    graphicsBinding.hGLRC = hglrc;  // Valid HGLRC handle
    
    /////////////////////////////////////////////////////////////////////////////////////////////

    XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};
    sessionCI.next = &graphicsBinding;                            
    sessionCI.createFlags = 0;
    sessionCI.systemId = m_systemID;

    OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCI, &m_session), "Failed to create Session.");
}

void OpenxrPlugIn::CreateActionPoses()
{
    // Create an xrSpace for a pose action.
    auto CreateActionPoseSpace = [this](XrSession session, XrAction xrAction, const char* subaction_path = nullptr) -> XrSpace
    {
        XrSpace xrSpace;
        const XrPosef xrPoseIdentity = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
        // Create frame of reference for a pose action
        XrActionSpaceCreateInfo actionSpaceCI{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceCI.action = xrAction;
        actionSpaceCI.poseInActionSpace = xrPoseIdentity;
        if (subaction_path) actionSpaceCI.subactionPath = CreateXrPath(subaction_path);
        OPENXR_CHECK(xrCreateActionSpace(session, &actionSpaceCI, &xrSpace), "Failed to create ActionSpace.");
        return xrSpace;
    };
    m_handPoseSpace[0] = CreateActionPoseSpace(m_session, m_palmPoseAction, "/user/hand/left");
    m_handPoseSpace[1] = CreateActionPoseSpace(m_session, m_palmPoseAction, "/user/hand/right");
}

void OpenxrPlugIn::AttachActionSet() 
{
    // Attach the action set we just made to the session. We could attach multiple action sets!
    XrSessionActionSetsAttachInfo actionSetAttachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    actionSetAttachInfo.countActionSets = 1;
    actionSetAttachInfo.actionSets = &m_actionSet;
    OPENXR_CHECK(xrAttachSessionActionSets(m_session, &actionSetAttachInfo), "Failed to attach ActionSet to Session.");
}

void OpenxrPlugIn::CreateReferenceSpace()
{
    // Fill out an XrReferenceSpaceCreateInfo structure and create a reference XrSpace, specifying a Local space with an
    // identity pose as the origin.
    XrReferenceSpaceCreateInfo referenceSpaceCI{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    //referenceSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    referenceSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    referenceSpaceCI.poseInReferenceSpace = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
    OPENXR_CHECK(xrCreateReferenceSpace(m_session, &referenceSpaceCI, &m_localSpace), "Failed to create ReferenceSpace.");
}

void OpenxrPlugIn::CreateSwapchains()
{
    // Get the supported swapchain formats as an array of int64_t and ordered by runtime preference.
    uint32_t formatCount = 0;
    OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr), "Failed to enumerate Swapchain Formats");
    std::vector<int64_t> formats(formatCount);
    OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data()),
                 "Failed to enumerate Swapchain Formats");
    //if (m_graphicsAPI->SelectDepthSwapchainFormat(formats) == 0)
    //{
    //    std::cerr << "Failed to find depth format for Swapchain." << std::endl;
    //    DEBUG_BREAK;
    //}

    // Resize the SwapchainInfo to match the number of view in the View Configuration.
    m_colorSwapchainInfos.resize(m_viewConfigurationViews.size());
    m_depthSwapchainInfos.resize(m_viewConfigurationViews.size());

    swapchainImages.resize(m_viewConfigurationViews.size());

    for (size_t i = 0; i < m_viewConfigurationViews.size(); i++)
    {
        SwapchainInfo& colorSwapchainInfo = m_colorSwapchainInfos[i];

        // Fill out an XrSwapchainCreateInfo structure and create an XrSwapchain.
        // Color.
        XrSwapchainCreateInfo swapchainCI{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainCI.createFlags = 0;
        swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCI.format = GL_SRGB8_ALPHA8;
        swapchainCI.sampleCount = m_viewConfigurationViews[i].recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
        swapchainCI.width = m_viewConfigurationViews[i].recommendedImageRectWidth;
        swapchainCI.height = m_viewConfigurationViews[i].recommendedImageRectHeight;
        swapchainCI.faceCount = 1;
        swapchainCI.arraySize = 1;
        swapchainCI.mipCount = 1;
        OPENXR_CHECK(xrCreateSwapchain(m_session, &swapchainCI, &colorSwapchainInfo.swapchain),
                     "Failed to create Color Swapchain");
        colorSwapchainInfo.swapchainFormat = swapchainCI.format;  // Save the swapchain format for later use.


        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        uint32_t colorSwapchainImageCount = 0;
        OPENXR_CHECK(xrEnumerateSwapchainImages(colorSwapchainInfo.swapchain, 0, &colorSwapchainImageCount, nullptr), "Failed to enumerate Color Swapchain Images.");  

        swapchainImages[i].resize(colorSwapchainImageCount);
        for (int a = 0; a < swapchainImages[i].size(); a++)
        {
            swapchainImages[i][a].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
        }

        OPENXR_CHECK(xrEnumerateSwapchainImages(colorSwapchainInfo.swapchain,
                                               colorSwapchainImageCount,
                                               &colorSwapchainImageCount,
                                                reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages[i].data())),
                     "Failed to enumerate Color Swapchain Images.");

         //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////        

    }
}



//Update

void OpenxrPlugIn::PollEvents() 
{
    // Poll OpenXR for a new event.
    XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
    auto XrPollEvents = [&]() -> bool
    {
        eventData = {XR_TYPE_EVENT_DATA_BUFFER};
        return xrPollEvent(m_xrInstance, &eventData) == XR_SUCCESS;
    };

    while (XrPollEvents())
    {
        switch (eventData.type)
        {
                // Log the number of lost events from the runtime.
            case XR_TYPE_EVENT_DATA_EVENTS_LOST:
            {
                XrEventDataEventsLost* eventsLost = reinterpret_cast<XrEventDataEventsLost*>(&eventData);
                XR_TUT_LOG("OPENXR: Events Lost: " << eventsLost->lostEventCount);
                break;
            }
            // Log that an instance loss is pending and shutdown the application.
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            {
                XrEventDataInstanceLossPending* instanceLossPending =
                    reinterpret_cast<XrEventDataInstanceLossPending*>(&eventData);
                XR_TUT_LOG("OPENXR: Instance Loss Pending at: " << instanceLossPending->lossTime);
                //m_sessionRunning = false;
                //m_applicationRunning = false;
                break;
            }
            // Log that the interaction profile has changed.
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
            {
                XrEventDataInteractionProfileChanged* interactionProfileChanged =
                    reinterpret_cast<XrEventDataInteractionProfileChanged*>(&eventData);
                XR_TUT_LOG("OPENXR: Interaction Profile changed for Session: " << interactionProfileChanged->session);
                if (interactionProfileChanged->session != m_session)
                {
                    XR_TUT_LOG("XrEventDataInteractionProfileChanged for unknown Session");
                    break;
                }
                RecordCurrentBindings();
                break;
            }
            // Log that there's a reference space change pending.
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
            {
                XrEventDataReferenceSpaceChangePending* referenceSpaceChangePending =
                    reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&eventData);
                XR_TUT_LOG("OPENXR: Reference Space Change pending for Session: " << referenceSpaceChangePending->session);
                if (referenceSpaceChangePending->session != m_session)
                {
                    XR_TUT_LOG("XrEventDataReferenceSpaceChangePending for unknown Session");
                    break;
                }
                break;
            }
            // Session State changes:
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
            {
                XrEventDataSessionStateChanged* sessionStateChanged =
                    reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                if (sessionStateChanged->session != m_session)
                {
                    XR_TUT_LOG("XrEventDataSessionStateChanged for unknown Session");
                    break;
                }

                if (sessionStateChanged->state == XR_SESSION_STATE_READY)
                {
                    // SessionState is ready. Begin the XrSession using the XrViewConfigurationType.
                    XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                    sessionBeginInfo.primaryViewConfigurationType = m_viewConfiguration;
                    OPENXR_CHECK(xrBeginSession(m_session, &sessionBeginInfo), "Failed to begin Session.");
                    //m_sessionRunning = true;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING)
                {
                    // SessionState is stopping. End the XrSession.
                    OPENXR_CHECK(xrEndSession(m_session), "Failed to end Session.");
                    //m_sessionRunning = false;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_EXITING)
                {
                    // SessionState is exiting. Exit the application.
                    //m_sessionRunning = false;
                    //m_applicationRunning = false;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING)
                {
                    // SessionState is loss pending. Exit the application.
                    // It's possible to try a reestablish an XrInstance and XrSession, but we will simply exit here.
                    //m_sessionRunning = false;
                    //m_applicationRunning = false;
                }
                // Store state for reference across the application.
                m_sessionState = sessionStateChanged->state;
                RecordCurrentBindings();
                break;
            }

            default:
            {
                break;
            }
        }
    }
}

void OpenxrPlugIn::RecordCurrentBindings() 
{
    if (m_session)
    {
        // now we are ready to:
        XrInteractionProfileState interactionProfile = {XR_TYPE_INTERACTION_PROFILE_STATE, 0, 0};
        // for each action, what is the binding?
        OPENXR_CHECK(xrGetCurrentInteractionProfile(m_session, m_handPaths[0], &interactionProfile), "Failed to get profile.");
        if (interactionProfile.interactionProfile)
        {
            XR_TUT_LOG("user/hand/left ActiveProfile " << FromXrPath(interactionProfile.interactionProfile).c_str());
        }
        OPENXR_CHECK(xrGetCurrentInteractionProfile(m_session, m_handPaths[1], &interactionProfile), "Failed to get profile.");
        if (interactionProfile.interactionProfile)
        {
            XR_TUT_LOG("user/hand/right ActiveProfile " << FromXrPath(interactionProfile.interactionProfile).c_str());
        }
    }
}

void OpenxrPlugIn::PollActions(XrTime predictedTime)

{
    // Update our action set with up-to-date input data.
    // First, we specify the actionSet we are polling.
    XrActiveActionSet activeActionSet{};
    activeActionSet.actionSet = m_actionSet;
    activeActionSet.subactionPath = XR_NULL_PATH;
    // Now we sync the Actions to make sure they have current data.
    XrActionsSyncInfo actionsSyncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    actionsSyncInfo.countActiveActionSets = 1;
    actionsSyncInfo.activeActionSets = &activeActionSet;
    OPENXR_CHECK(xrSyncActions(m_session, &actionsSyncInfo), "Failed to sync Actions.");

    XrActionStateGetInfo actionStateGetInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    // We pose a single Action, twice - once for each subAction Path.
    actionStateGetInfo.action = m_palmPoseAction;
    // For each hand, get the pose state if possible.
    for (int i = 0; i < 2; i++)
    {
        // Specify the subAction Path.
        actionStateGetInfo.subactionPath = m_handPaths[i];
        OPENXR_CHECK(xrGetActionStatePose(m_session, &actionStateGetInfo, &m_handPoseState[i]), "Failed to get Pose State.");
        if (m_handPoseState[i].isActive)
        {
            XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
            XrResult res = xrLocateSpace(m_handPoseSpace[i], m_localSpace, predictedTime, &spaceLocation);
            if (XR_UNQUALIFIED_SUCCESS(res) && (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0)
            {
                m_handPose[i] = spaceLocation.pose;
            }
            else
            {
                m_handPoseState[i].isActive = false;
            }
        }
    }

    for (int i = 0; i < 2; i++)
    {
        actionStateGetInfo.action = m_grabCubeAction;
        actionStateGetInfo.subactionPath = m_handPaths[i];
        OPENXR_CHECK(xrGetActionStateFloat(m_session, &actionStateGetInfo, &m_grabState[i]),
                     "Failed to get Float State of Grab Cube action.");
    }
    for (int i = 0; i < 2; i++)
    {
        actionStateGetInfo.action = m_changeColorAction;
        actionStateGetInfo.subactionPath = m_handPaths[i];
        OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &m_changeColorState[i]),
                     "Failed to get Boolean State of Change Color action.");
    }
    // The Spawn Cube action has no subActionPath:
    {
        actionStateGetInfo.action = m_spawnCubeAction;
        actionStateGetInfo.subactionPath = 0;
        OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &m_spawnCubeState),
                     "Failed to get Boolean State of Spawn Cube action.");
    }

    for (int i = 0; i < 2; i++)
    {
        m_buzz[i] *= 0.5f;
        if (m_buzz[i] < 0.01f) m_buzz[i] = 0.0f;
        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
        vibration.amplitude = m_buzz[i];
        vibration.duration = XR_MIN_HAPTIC_DURATION;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
        hapticActionInfo.action = m_buzzAction;
        hapticActionInfo.subactionPath = m_handPaths[i];
        OPENXR_CHECK(xrApplyHapticFeedback(m_session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration),
                     "Failed to apply haptic feedback.");
    }

    //INPUT ABSTRACTION

    //Left
    {        
        actionStateGetInfo.action = leftTrigger;
        OPENXR_CHECK(xrGetActionStateFloat(m_session, &actionStateGetInfo, &leftTriggerState),
                     "Failed to get Float State of leftTrigger.");

        actionStateGetInfo.action = leftGrip;
        OPENXR_CHECK(xrGetActionStateFloat(m_session, &actionStateGetInfo, &leftGripState),
                     "Failed to get Float State of leftGrip.");

        actionStateGetInfo.action = x_button;
        OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &x_buttonState),
                     "Failed to get Float State of x_button.");

        actionStateGetInfo.action = y_button;
        OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &y_buttonState),
                     "Failed to get Float State of y_button.")

        actionStateGetInfo.action = leftThumbstick_click;
        OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &leftThumbstick_clickState),
                     "Failed to get Float State of left thumbstick click.")

        actionStateGetInfo.action = leftJoystick_x;
        OPENXR_CHECK(xrGetActionStateFloat(m_session, &actionStateGetInfo, &leftJoystick_x_State),
                     "Failed to get Float State of leftJoystick x.");

        actionStateGetInfo.action = leftJoystick_y;
        OPENXR_CHECK(xrGetActionStateFloat(m_session, &actionStateGetInfo, &leftJoystick_y_State),
                     "Failed to get Float State of leftJoystick y.");
    }

    // Right
    {
        actionStateGetInfo.action = rightTrigger;
        OPENXR_CHECK(xrGetActionStateFloat(m_session, &actionStateGetInfo, &rightTriggerState),
                     "Failed to get Float State of rightTrigger.");

        actionStateGetInfo.action = rightGrip;
        OPENXR_CHECK(xrGetActionStateFloat(m_session, &actionStateGetInfo, &rightGripState),
                     "Failed to get Float State of rightGrip.");

        actionStateGetInfo.action = a_button;
        OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &a_buttonState),
                     "Failed to get Float State of a_button.");

        actionStateGetInfo.action = b_button;
        OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &b_buttonState),
                     "Failed to get Float State of b_button.")

        actionStateGetInfo.action = rightThumbstick_click;
        OPENXR_CHECK(xrGetActionStateBoolean(m_session, &actionStateGetInfo, &rightThumbstick_clickState),
                     "Failed to get Float State of right thumbstick click.")

        actionStateGetInfo.action = rightJoystick_x;
        OPENXR_CHECK(xrGetActionStateFloat(m_session, &actionStateGetInfo, &rightJoystick_x_State),
                     "Failed to get Float State of rightJoystick x.");

        actionStateGetInfo.action = rightJoystick_y;
        OPENXR_CHECK(xrGetActionStateFloat(m_session, &actionStateGetInfo, &rightJoystick_y_State),
                     "Failed to get Float State of rightJoystick y.");
    }
}

void OpenxrPlugIn::GetControllerPose(int controllerIndx, float& pos_x, float& pos_y, float& pos_z, glm::quat& rot)
{ 
    glm::vec3 pos;
    XrVector3f_To_glm_vec3(pos, m_handPose[controllerIndx].position); 
    pos_x = pos.x;
    pos_y = pos.y;
    pos_z = pos.z;

    glm::vec4 r;
    XrQuaternionf_To_glm_vec4(r, m_handPose[controllerIndx].orientation);
    rot = glm::quat(r.w, r.x, r.y, r.z);
}



//Render

void OpenxrPlugIn::RenderXRBeguin() 
{
    PollEvents();

    // Get the XrFrameState for timing and rendering info.
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    OPENXR_CHECK(xrWaitFrame(m_session, &frameWaitInfo, &frameState), "Failed to wait for XR Frame.");

    // Tell the OpenXR compositor that the application is beginning the frame.
    XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    OPENXR_CHECK(xrBeginFrame(m_session, &frameBeginInfo), "Failed to begin the XR Frame.");

    // Variables for rendering and layer composition.
    bool rendered = false;
    RenderLayerInfo renderLayerInfo;
    renderLayerInfo.predictedDisplayTime = frameState.predictedDisplayTime;

    // Check that the session is active and that we should render.
    bool sessionActive = (m_sessionState == XR_SESSION_STATE_SYNCHRONIZED || m_sessionState == XR_SESSION_STATE_VISIBLE ||
                          m_sessionState == XR_SESSION_STATE_FOCUSED);
    if (sessionActive && frameState.shouldRender)
    {
        // poll actions here because they require a predicted display time, which we've only just obtained.
        PollActions(frameState.predictedDisplayTime);

        // APP things   -    Handle the interaction between the user and the 3D blocks.



        // Render the stereo image and associate one of swapchain images with the XrCompositionLayerProjection structure.
        rendered = RenderLayer(renderLayerInfo);
        if (rendered)
        {
            renderLayerInfo.layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&renderLayerInfo.layerProjection));
        }
    }

    // Tell OpenXR that we are finished with this frame; specifying its display time, environment blending and layers.
    XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
    frameEndInfo.displayTime = frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = m_environmentBlendMode;
    frameEndInfo.layerCount = static_cast<uint32_t>(renderLayerInfo.layers.size());
    frameEndInfo.layers = renderLayerInfo.layers.data();
    OPENXR_CHECK(xrEndFrame(m_session, &frameEndInfo), "Failed to end the XR Frame.");
}

glm::vec3 recordedCameraPos;
glm::quat recordedCameraRot;


bool OpenxrPlugIn::RenderLayer(RenderLayerInfo& renderLayerInfo) 
{
    // Locate the views from the view configuration within the (reference) space at the display time.
    std::vector<XrView> views(m_viewConfigurationViews.size(), {XR_TYPE_VIEW});

    XrViewState viewState{
        XR_TYPE_VIEW_STATE};  // Will contain information on whether the position and/or orientation is valid and/or tracked.
    XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
    viewLocateInfo.viewConfigurationType = m_viewConfiguration;
    viewLocateInfo.displayTime = renderLayerInfo.predictedDisplayTime;
    viewLocateInfo.space = m_localSpace;
    uint32_t viewCount = 0;
    xrLocateViews(m_session, &viewLocateInfo, &viewState, static_cast<uint32_t>(views.size()), &viewCount, views.data());

    // Resize the layer projection views to match the view count. The layer projection views are used in the layer projection.
    renderLayerInfo.layerProjectionViews.resize(viewCount, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

    // AR
    renderLayerInfo.layerDepthInfos.resize(viewCount, {XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR});

    // Per view in the view configuration:

    ////Record camera pos and rot to apply correctly the local tracking offset and turn it back to the origin transform.
    //for (const auto& [e, camera, cameraTransform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
    //{
    //    recordedCameraPos = cameraTransform.GetTranslation();
    //    recordedCameraRot = cameraTransform.GetRotation();
    //}

    for (uint32_t i = 0; i < viewCount; i++)
    {
        eyeIndx = i;
        
        SwapchainInfo& colorSwapchainInfo = m_colorSwapchainInfos[i];

        // Acquire and wait for an image from the swapchains.
        // Get the image index of an image in the swapchains.
        // The timeout is infinite.
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        OPENXR_CHECK(xrAcquireSwapchainImage(colorSwapchainInfo.swapchain, &acquireInfo, &swapchainImageIndx),
                     "Failed to acquire Image from the Color Swapchian");
        //OPENXR_CHECK(xrAcquireSwapchainImage(depthSwapchainInfo.swapchain, &acquireInfo, &depthImageIndex),
        //             "Failed to acquire Image from the Depth Swapchian");

        XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        OPENXR_CHECK(xrWaitSwapchainImage(colorSwapchainInfo.swapchain, &waitInfo),
                     "Failed to wait for Image from the Color Swapchain");
        //OPENXR_CHECK(xrWaitSwapchainImage(depthSwapchainInfo.swapchain, &waitInfo),
        //             "Failed to wait for Image from the Depth Swapchain");

        // Get the width and height and construct the viewport and scissors.
        const uint32_t& width = m_viewConfigurationViews[i].recommendedImageRectWidth;
        const uint32_t& height = m_viewConfigurationViews[i].recommendedImageRectHeight;
        //GraphicsAPI::Viewport viewport = {0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
        //GraphicsAPI::Rect2D scissor = {{(int32_t)0, (int32_t)0}, {width, height}};
        float nearZ = 0.05f;
        float farZ = 100.0f;

        // Fill out the XrCompositionLayerProjectionView structure specifying the pose and fov from the view. This also
        // associates the swapchain image with this layer projection view.
        renderLayerInfo.layerProjectionViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        renderLayerInfo.layerProjectionViews[i].pose = views[i].pose;
        renderLayerInfo.layerProjectionViews[i].fov = views[i].fov;
        renderLayerInfo.layerProjectionViews[i].subImage.swapchain = colorSwapchainInfo.swapchain;
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.x = 0;
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.y = 0;
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.width = static_cast<int32_t>(width);
        renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.height = static_cast<int32_t>(height);
        renderLayerInfo.layerProjectionViews[i].subImage.imageArrayIndex = 0;  // Useful for multiview rendering.

        //// AR
        //renderLayerInfo.layerProjectionViews[i].next = &renderLayerInfo.layerDepthInfos[i];

        //renderLayerInfo.layerDepthInfos[i] = {XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR};
        //renderLayerInfo.layerDepthInfos[i].subImage.swapchain = depthSwapchainInfo.swapchain;
        //renderLayerInfo.layerDepthInfos[i].subImage.imageRect.offset.x = 0;
        //renderLayerInfo.layerDepthInfos[i].subImage.imageRect.offset.y = 0;
        //renderLayerInfo.layerDepthInfos[i].subImage.imageRect.extent.width = static_cast<int32_t>(width);
        //renderLayerInfo.layerDepthInfos[i].subImage.imageRect.extent.height = static_cast<int32_t>(height);
        //renderLayerInfo.layerDepthInfos[i].minDepth = 0;
        //renderLayerInfo.layerDepthInfos[i].maxDepth = 1;
        //renderLayerInfo.layerDepthInfos[i].nearZ = nearZ;
        //renderLayerInfo.layerDepthInfos[i].farZ = farZ;



        /////////////////////////////////////////////////////   RENDERING   //////////////////////////////////////////////////////////////////////////////////////////////

        //Camera xr actions                 

        glm::vec3 eyeLocalPos;
        glm::vec4 eyeLocalRot;
        XrVector3f_To_glm_vec3(eyeLocalPos, views[i].pose.position);
        XrQuaternionf_To_glm_vec4(eyeLocalRot, views[i].pose.orientation);

        glm::vec3 eyeWorldPos;
        glm::quat eyeWorldRot;

        for (const auto& [e, camera, cameraTransform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
        {
            //Rot and pos

            eyeWorldPos = eyeLocalPos;
            eyeWorldRot = glm::quat(eyeLocalRot.w, eyeLocalRot.x, eyeLocalRot.y, eyeLocalRot.z);

            cameraTransform.SetRotation(eyeWorldRot);
            cameraTransform.SetTranslation(eyeWorldPos);

            //Projection
            XrMatrix4x4f xrProj; 
            XrMatrix4x4f_CreateProjectionFov(&xrProj, views[i].fov, nearZ, farZ);
            XrMatrix4x4f_To_glm_mat4x4(camera.Projection, xrProj);               
        }

        //Rendering call

        bee::RendererXR& rendererxr = bee::Engine.ECS().GetSystem<bee::RendererXR>();

        rendererxr.RenderFlat(); 

        GLuint finalBufferIndx = rendererxr.m_finalFramebuffer;
        GLuint finalTextureWidth = rendererxr.m_width;
        GLuint finalTextureHeight = rendererxr.m_height;
        BlitToSwapchain(i, finalBufferIndx, finalTextureWidth, finalTextureHeight);


        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // Give the swapchain image back to OpenXR, allowing the compositor to use the image.
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        OPENXR_CHECK(xrReleaseSwapchainImage(colorSwapchainInfo.swapchain, &releaseInfo),
                     "Failed to release Image back to the Color Swapchain");
        //OPENXR_CHECK(xrReleaseSwapchainImage(depthSwapchainInfo.swapchain, &releaseInfo),
        //             "Failed to release Image back to the Depth Swapchain");
    }

    // Fill out the XrCompositionLayerProjection structure for usage with xrEndFrame().
    renderLayerInfo.layerProjection.layerFlags =
        XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    renderLayerInfo.layerProjection.space = m_localSpace;
    renderLayerInfo.layerProjection.viewCount = static_cast<uint32_t>(renderLayerInfo.layerProjectionViews.size());
    renderLayerInfo.layerProjection.views = renderLayerInfo.layerProjectionViews.data();

    ////Give back recorded pos and rot to the camera transform and let the game logic clean:
    //for (const auto& [e, camera, cameraTransform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
    //{
    //    cameraTransform.SetTranslation(recordedCameraPos);
    //    cameraTransform.SetRotation(recordedCameraRot);
    //} 

    return true;
}

void OpenxrPlugIn::BlitToSwapchain(int eyeIndex, int finalBufferIndx, int finalBufferTextureWidth, int finalBufferTextureHeight)
{
    // XR SWAPCHAINS FROM M_finalFramebuffer!!!!
    (void)eyeIndex;

    GLuint swchindx = swapchainImageIndx;                                  // INTRUSION
    GLuint indx = swapchainImages[eyeIndx][swchindx].image;  // INTRUSION
                                                                                                    //   |
    glBindFramebuffer(GL_READ_FRAMEBUFFER, finalBufferIndx);  //  V

    // Create and bind a new framebuffer for the swapchain texture
    GLuint swapchainFramebuffer;
    glGenFramebuffers(1, &swapchainFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, swapchainFramebuffer);

    // Attach the swapchain image to the new framebuffer
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, indx, 0);

    // Check if the framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &swapchainFramebuffer);
        return;  // Abort blit operation if framebuffer is not complete
    }

    /////////////////////////////////////////// VR BLITTING
    ////////////////////////////////////////////////////////////////////////

    // int sourceWidth = m_width;
    // int sourceHeight = m_height;

    //// Destination region (centered adjustment)
    // int destWidth = Engine.ECS().GetSystem<OpenxrPlugIn>().m_viewConfigurationViews[eyeIndx].recommendedImageRectWidth;
    // int destHeight = Engine.ECS().GetSystem<OpenxrPlugIn>().m_viewConfigurationViews[eyeIndx].recommendedImageRectHeight;

    //// Retrieve FOV angles (from OpenXR views[i].fov for each eye)
    // float tanLeft = tanf(fov.angleLeft);
    // float tanRight = tanf(fov.angleRight);
    // float tanUp = tanf(fov.angleUp);
    // float tanDown = tanf(fov.angleDown);

    //// Compute the normalized offsets for the visual center in NDC
    // float centerX_NDC = (tanRight + tanLeft) / (2.0f * (tanRight - tanLeft));
    // float centerY_NDC = (tanUp + tanDown) / (2.0f * (tanUp - tanDown));

    //// Convert NDC center offsets to pixel coordinates
    // int blitXOffset = (int)((centerX_NDC + 0.5f) * destWidth - sourceWidth / 2.0f);
    // int blitYOffset = (int)((centerY_NDC + 0.5f) * destHeight - sourceHeight / 2.0f);

    // Perform the adjusted blit
    glBlitFramebuffer(0,
                      0,
        finalBufferTextureWidth,
        finalBufferTextureHeight,  // Source rectangle
                      0,
                      0,                    // Adjusted destination rectangle (bottom-left corner)
                      m_viewConfigurationViews[eyeIndx].recommendedImageRectWidth,  // Width aligned with the projection center
                      m_viewConfigurationViews[eyeIndx].recommendedImageRectHeight,  // Height aligned with the projection center
                      GL_COLOR_BUFFER_BIT,  // Buffer to copy
                      GL_NEAREST            // Sampling filter
    );
    // Perform the blit
    // glBlitFramebuffer(0, 0, sourceWidth, sourceHeight, xOffset, yOffset, destWidth - xOffset, destHeight - yOffset,
    // GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Clean up
    glBindFramebuffer(GL_FRAMEBUFFER, finalBufferIndx);            // Unbind both framebuffers
    glDeleteFramebuffers(1, &swapchainFramebuffer);  // Delete the temporary framebuffer
}

void OpenxrPlugIn::RenderXREnd() 
{
    
}




