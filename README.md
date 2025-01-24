# OpenXRPlugIn-for-Bee
Using OpenXR, it add  a decoupled VR implementation to the Bee game engine (Breda university of applied Science)
![image](https://github.com/user-attachments/assets/89c13235-9cbb-4edd-84ff-91df3a5abeb6)

Take a look at the Blog where more insights are explained:


## Installation in Bee
Version 0.2, little changes to avoid warmings.

1. Import the openxr folder into: bee/external.

2. Import the bee_XR_debug.props and bee_XR_release.props in bee/properties.

3. In visual studio add this properties in bee x64 debug / release and in you app project x64 debug / release as well. Also include every file from the openxr folder in the project.

4. wherever you want to access the plug-in, add the following includes. V0.2-> IMPORTANT: OpenXRPlugIn includes should be the last ones to avoid errors.
```cpp
//XR includes
#include "openxrPlugIn.h"       
#include "renderXR_gl.h"        
#include "xrComponents.h"

...

```
5. To initialize the plug in and create a xr instance use the following code for a common set up. It initialices the plug-in, stablish the slightly modified version of the current Bee renderer ( rendererXR), initialice the xrcomponent system:

```cpp
//XR Essential entities

Engine.ECS().CreateSystem<OpenxrPlugIn>().Init();            
Engine.ECS().CreateSystem<bee::XRComponentsSystem>();
Engine.ECS().CreateSystem<RendererXR>(); 

Entity xrRigEntity;
Entity xrCameraEntity;
Entity lHandEntity;
Entity rHandEntity;

Engine.ECS().GetSystem<XRComponentsSystem>().CreateXRRig(xrRigEntity, xrCameraEntity, lHandEntity, rHandEntity);

...

```
6. Now  we need a camera and the controllers to be able to do interesting interactions.
The plug-in works with 1 normal bee camera so it can be created manually, but xrComponents gives easier methos to directly set the proper entity hierarchy. XRrig is a term for a common structure IT consist of a parent that has the tracked camera and controllers as childs for proper placement:

```cpp
Entity xrRigEntity;
Entity xrCameraEntity;
Entity lHandEntity;
Entity rHandEntity;

Engine.ECS().GetSystem<XRComponentsSystem>().CreateXRRig(xrRigEntity, xrCameraEntity, lHandEntity, rHandEntity);

//For attach Models to your controllers just create a new entity with the model and make it child of the hand entities

...

```
After that you should be able to see the scene. Initially your XRRig will be placed in world origin (0,0,0). You will be able to see from that perspective, accounting also your real world position .

7. Last, for input, each controller comes with a component that provides methods to access the inputs ( position, orientation, buttons, joysticks). To access it, a common ECS registry view  with the component XRController during the update  will give you access to it. Take in consideration, Position and orientation of the XRController entity are handled automatically in XRComponentSystem:

```cpp
//Accessing Input

for (const auto& [e, xrController, transform] : bee::Engine.ECS().Registry.view<bee::XRController, bee::Transform>().each())
{
    if (e == lHand)
    {
        bool xPressed = xrController.Get_a_x_Value();    
    }
    if (e == rHand)
    {
        float joystickXValue = xrControllerGetJoystick_x_Value();

        //glm::vec3 position = xrControllerGetPosition 
        glm::vec3 position = transform.GetTranslation();  //<- better practice 
     }

}

...

```
After that all is ready to create some simple apps. However more improvements are needed and problems could arise when trying more advance interactions.
