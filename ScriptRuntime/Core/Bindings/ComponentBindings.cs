using Pine.Math;

namespace Pine.Core.Bindings
{
    // The engine functions behind Pine.World.Component and everything under
    // Pine.World.Components. Grouped, named and ordered as
    // Pine::Script::Interfaces::Component::Setup registers them, so the two lists can be read
    // side by side.
    //
    // A predicate answers with a byte rather than a bool: the engine returns a one-byte C++ bool
    // and the default marshalling of a managed bool is four bytes wide, so the upper three would
    // be whatever happened to be in the register. Anything that would be an object answers with a
    // handle, which Interop.ObjectFrom turns back into one on this side.
    internal static unsafe class ComponentBindings
    {
        public static delegate* unmanaged<uint, int, byte> GetActive;
        public static delegate* unmanaged<uint, int, byte, void> SetActive;

        public static delegate* unmanaged<uint, UId, void> SetModel;
        public static delegate* unmanaged<uint, ulong> GetModel;
        public static delegate* unmanaged<uint, byte> ModelRendererGetCastShadows;
        public static delegate* unmanaged<uint, byte, void> ModelRendererSetCastShadows;
        public static delegate* unmanaged<uint, byte> ModelRendererGetReceiveShadows;
        public static delegate* unmanaged<uint, byte, void> ModelRendererSetReceiveShadows;

        public static delegate* unmanaged<uint, Vector3*, int, void> RigidBodyApplyForce;

        public static delegate* unmanaged<uint, Vector3*, void> CharacterControllerMove;
        public static delegate* unmanaged<uint, Vector3*, void> CharacterControllerSetPosition;
        public static delegate* unmanaged<uint, byte> CharacterControllerIsGrounded;
        public static delegate* unmanaged<uint, byte> CharacterControllerIsTouchingSides;
        public static delegate* unmanaged<uint, Vector3*, void> CharacterControllerGetVelocity;
        public static delegate* unmanaged<uint, float, void> CharacterControllerSetVerticalVelocity;
        public static delegate* unmanaged<uint, float> CharacterControllerGetVerticalVelocity;

        public static delegate* unmanaged<uint, Vector3*, void> TransformGetPosition;
        public static delegate* unmanaged<uint, Quaternion*, void> TransformGetRotation;
        public static delegate* unmanaged<uint, Vector3*, void> TransformGetScale;
        public static delegate* unmanaged<uint, Vector3*, void> TransformGetLocalPosition;
        public static delegate* unmanaged<uint, Vector3*, void> TransformSetLocalPosition;
        public static delegate* unmanaged<uint, Quaternion*, void> TransformGetLocalRotation;
        public static delegate* unmanaged<uint, Quaternion*, void> TransformSetLocalRotation;
        public static delegate* unmanaged<uint, Vector3*, void> TransformSetLocalEulerAngles;
        public static delegate* unmanaged<uint, Vector3*, void> TransformGetLocalEulerAngles;
        public static delegate* unmanaged<uint, Vector3*, void> TransformGetLocalScale;
        public static delegate* unmanaged<uint, Vector3*, void> TransformSetLocalScale;
        public static delegate* unmanaged<uint, Vector3*, void> TransformGetUp;
        public static delegate* unmanaged<uint, Vector3*, void> TransformGetRight;
        public static delegate* unmanaged<uint, Vector3*, void> TransformGetForward;

        public static delegate* unmanaged<uint, int> LightGetLightType;
        public static delegate* unmanaged<uint, int, void> LightSetLightType;
        public static delegate* unmanaged<uint, Vector3*, void> LightGetLightColor;
        public static delegate* unmanaged<uint, Vector3*, void> LightSetLightColor;
        public static delegate* unmanaged<uint, float> LightGetLightIntensity;
        public static delegate* unmanaged<uint, float, void> LightSetLightIntensity;
        public static delegate* unmanaged<uint, float> LightGetRange;
        public static delegate* unmanaged<uint, float, void> LightSetRange;
        public static delegate* unmanaged<uint, byte> LightGetCastShadows;
        public static delegate* unmanaged<uint, byte, void> LightSetCastShadows;
        public static delegate* unmanaged<uint, float> LightGetSpotlightOuterAngle;
        public static delegate* unmanaged<uint, float, void> LightSetSpotlightOuterAngle;
        public static delegate* unmanaged<uint, float> LightGetSpotlightInnerAngle;
        public static delegate* unmanaged<uint, float, void> LightSetSpotlightInnerAngle;

        public static delegate* unmanaged<uint, int> CameraGetCameraType;
        public static delegate* unmanaged<uint, int, void> CameraSetCameraType;
        public static delegate* unmanaged<uint, float> CameraGetNearPlane;
        public static delegate* unmanaged<uint, float, void> CameraSetNearPlane;
        public static delegate* unmanaged<uint, float> CameraGetFarPlane;
        public static delegate* unmanaged<uint, float, void> CameraSetFarPlane;
        public static delegate* unmanaged<uint, float> CameraGetFieldOfView;
        public static delegate* unmanaged<uint, float, void> CameraSetFieldOfView;
        public static delegate* unmanaged<uint, float> CameraGetOrthographicSize;
        public static delegate* unmanaged<uint, float, void> CameraSetOrthographicSize;
        public static delegate* unmanaged<uint, Vector4*, void> CameraGetClearColor;
        public static delegate* unmanaged<uint, Vector4*, void> CameraSetClearColor;
        public static delegate* unmanaged<uint, Vector3*, Vector3*, void> CameraWorldToScreenPoint;

        public static delegate* unmanaged<uint, int> ColliderGetColliderType;
        public static delegate* unmanaged<uint, int, void> ColliderSetColliderType;
        public static delegate* unmanaged<uint, Vector3*, void> ColliderGetPosition;
        public static delegate* unmanaged<uint, Vector3*, void> ColliderSetPosition;
        public static delegate* unmanaged<uint, Vector3*, void> ColliderGetSize;
        public static delegate* unmanaged<uint, Vector3*, void> ColliderSetSize;
        public static delegate* unmanaged<uint, float> ColliderGetRadius;
        public static delegate* unmanaged<uint, float, void> ColliderSetRadius;
        public static delegate* unmanaged<uint, float> ColliderGetHeight;
        public static delegate* unmanaged<uint, float, void> ColliderSetHeight;
        public static delegate* unmanaged<uint, uint> ColliderGetLayer;
        public static delegate* unmanaged<uint, uint, void> ColliderSetLayer;
        public static delegate* unmanaged<uint, uint> ColliderGetLayerMask;
        public static delegate* unmanaged<uint, uint, void> ColliderSetLayerMask;
        public static delegate* unmanaged<uint, byte> ColliderGetIsTrigger;
        public static delegate* unmanaged<uint, byte, void> ColliderSetIsTrigger;
        public static delegate* unmanaged<uint, uint> ColliderGetTriggerMask;
        public static delegate* unmanaged<uint, uint, void> ColliderSetTriggerMask;

        public static delegate* unmanaged<uint, ulong> AudioSourceGetAudioFile;
        public static delegate* unmanaged<uint, UId, void> AudioSourceSetAudioFile;
        public static delegate* unmanaged<uint, void> AudioSourcePlay;
        public static delegate* unmanaged<uint, void> AudioSourcePause;
        public static delegate* unmanaged<uint, void> AudioSourceStop;
        public static delegate* unmanaged<uint, int> AudioSourceGetPlaybackState;
        public static delegate* unmanaged<uint, byte> AudioSourceIsPlaying;
        public static delegate* unmanaged<uint, byte> AudioSourceGetPlayOnStart;
        public static delegate* unmanaged<uint, byte, void> AudioSourceSetPlayOnStart;
        public static delegate* unmanaged<uint, byte> AudioSourceGetLoop;
        public static delegate* unmanaged<uint, byte, void> AudioSourceSetLoop;
        public static delegate* unmanaged<uint, byte> AudioSourceGetSpatial;
        public static delegate* unmanaged<uint, byte, void> AudioSourceSetSpatial;
        public static delegate* unmanaged<uint, float> AudioSourceGetVolume;
        public static delegate* unmanaged<uint, float, void> AudioSourceSetVolume;
        public static delegate* unmanaged<uint, float> AudioSourceGetPitch;
        public static delegate* unmanaged<uint, float, void> AudioSourceSetPitch;
        public static delegate* unmanaged<uint, float> AudioSourceGetReferenceDistance;
        public static delegate* unmanaged<uint, float, void> AudioSourceSetReferenceDistance;
        public static delegate* unmanaged<uint, float> AudioSourceGetMaxDistance;
        public static delegate* unmanaged<uint, float, void> AudioSourceSetMaxDistance;
        public static delegate* unmanaged<uint, float> AudioSourceGetRolloffFactor;
        public static delegate* unmanaged<uint, float, void> AudioSourceSetRolloffFactor;
        public static delegate* unmanaged<uint, float> AudioSourceGetPlaybackPosition;
        public static delegate* unmanaged<uint, float, void> AudioSourceSetPlaybackPosition;

        public static delegate* unmanaged<uint, float> AudioListenerGetVolume;
        public static delegate* unmanaged<uint, float, void> AudioListenerSetVolume;

        public static delegate* unmanaged<uint, ulong> ScriptGetCSharpScript;
        public static delegate* unmanaged<uint, ulong> ScriptGetInstance;

        public static void Bind()
        {
            GetActive = (delegate* unmanaged<uint, int, byte>)Interop.Resolve("Pine.World.Component::GetActive");
            SetActive = (delegate* unmanaged<uint, int, byte, void>)Interop.Resolve("Pine.World.Component::SetActive");

            SetModel = (delegate* unmanaged<uint, UId, void>)Interop.Resolve("Pine.World.Components.ModelRenderer::SetModel");
            GetModel = (delegate* unmanaged<uint, ulong>)Interop.Resolve("Pine.World.Components.ModelRenderer::GetModel");
            ModelRendererGetCastShadows = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Components.ModelRenderer::PineGetCastShadows");
            ModelRendererSetCastShadows = (delegate* unmanaged<uint, byte, void>)Interop.Resolve("Pine.World.Components.ModelRenderer::PineSetCastShadows");
            ModelRendererGetReceiveShadows = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Components.ModelRenderer::PineGetReceiveShadows");
            ModelRendererSetReceiveShadows = (delegate* unmanaged<uint, byte, void>)Interop.Resolve("Pine.World.Components.ModelRenderer::PineSetReceiveShadows");

            RigidBodyApplyForce = (delegate* unmanaged<uint, Vector3*, int, void>)Interop.Resolve("Pine.World.Components.RigidBody::ApplyForce");

            CharacterControllerMove = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.CharacterController::PineMove");
            CharacterControllerSetPosition = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.CharacterController::PineSetPosition");
            CharacterControllerIsGrounded = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Components.CharacterController::PineIsGrounded");
            CharacterControllerIsTouchingSides = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Components.CharacterController::PineIsTouchingSides");
            CharacterControllerGetVelocity = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.CharacterController::PineGetVelocity");
            CharacterControllerSetVerticalVelocity = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.CharacterController::PineSetVerticalVelocity");
            CharacterControllerGetVerticalVelocity = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.CharacterController::PineGetVerticalVelocity");

            TransformGetPosition = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::GetPosition");
            TransformGetRotation = (delegate* unmanaged<uint, Quaternion*, void>)Interop.Resolve("Pine.World.Components.Transform::GetRotation");
            TransformGetScale = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::GetScale");
            TransformGetLocalPosition = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::GetLocalPosition");
            TransformSetLocalPosition = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::SetLocalPosition");
            TransformGetLocalRotation = (delegate* unmanaged<uint, Quaternion*, void>)Interop.Resolve("Pine.World.Components.Transform::GetLocalRotation");
            TransformSetLocalRotation = (delegate* unmanaged<uint, Quaternion*, void>)Interop.Resolve("Pine.World.Components.Transform::SetLocalRotation");
            TransformSetLocalEulerAngles = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::SetLocalEulerAngles");
            TransformGetLocalEulerAngles = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::GetLocalEulerAngles");
            TransformGetLocalScale = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::GetLocalScale");
            TransformSetLocalScale = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::SetLocalScale");
            TransformGetUp = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::GetUp");
            TransformGetRight = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::GetRight");
            TransformGetForward = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Transform::GetForward");

            LightGetLightType = (delegate* unmanaged<uint, int>)Interop.Resolve("Pine.World.Components.Light::PineGetLightType");
            LightSetLightType = (delegate* unmanaged<uint, int, void>)Interop.Resolve("Pine.World.Components.Light::PineSetLightType");
            LightGetLightColor = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Light::PineGetLightColor");
            LightSetLightColor = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Light::PineSetLightColor");
            LightGetLightIntensity = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.Light::PineGetLightIntensity");
            LightSetLightIntensity = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.Light::PineSetLightIntensity");
            LightGetRange = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.Light::PineGetRange");
            LightSetRange = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.Light::PineSetRange");
            LightGetCastShadows = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Components.Light::PineGetCastShadows");
            LightSetCastShadows = (delegate* unmanaged<uint, byte, void>)Interop.Resolve("Pine.World.Components.Light::PineSetCastShadows");
            LightGetSpotlightOuterAngle = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.Light::PineGetSpotlightOuterAngle");
            LightSetSpotlightOuterAngle = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.Light::PineSetSpotlightOuterAngle");
            LightGetSpotlightInnerAngle = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.Light::PineGetSpotlightInnerAngle");
            LightSetSpotlightInnerAngle = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.Light::PineSetSpotlightInnerAngle");

            CameraGetCameraType = (delegate* unmanaged<uint, int>)Interop.Resolve("Pine.World.Components.Camera::PineGetCameraType");
            CameraSetCameraType = (delegate* unmanaged<uint, int, void>)Interop.Resolve("Pine.World.Components.Camera::PineSetCameraType");
            CameraGetNearPlane = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.Camera::PineGetNearPlane");
            CameraSetNearPlane = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.Camera::PineSetNearPlane");
            CameraGetFarPlane = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.Camera::PineGetFarPlane");
            CameraSetFarPlane = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.Camera::PineSetFarPlane");
            CameraGetFieldOfView = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.Camera::PineGetFieldOfView");
            CameraSetFieldOfView = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.Camera::PineSetFieldOfView");
            CameraGetOrthographicSize = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.Camera::PineGetOrthographicSize");
            CameraSetOrthographicSize = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.Camera::PineSetOrthographicSize");
            CameraGetClearColor = (delegate* unmanaged<uint, Vector4*, void>)Interop.Resolve("Pine.World.Components.Camera::PineGetClearColor");
            CameraSetClearColor = (delegate* unmanaged<uint, Vector4*, void>)Interop.Resolve("Pine.World.Components.Camera::PineSetClearColor");
            CameraWorldToScreenPoint = (delegate* unmanaged<uint, Vector3*, Vector3*, void>)Interop.Resolve("Pine.World.Components.Camera::PineWorldToScreenPoint");

            ColliderGetColliderType = (delegate* unmanaged<uint, int>)Interop.Resolve("Pine.World.Components.Collider::PineGetColliderType");
            ColliderSetColliderType = (delegate* unmanaged<uint, int, void>)Interop.Resolve("Pine.World.Components.Collider::PineSetColliderType");
            ColliderGetPosition = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Collider::PineGetPosition");
            ColliderSetPosition = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Collider::PineSetPosition");
            ColliderGetSize = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Collider::PineGetSize");
            ColliderSetSize = (delegate* unmanaged<uint, Vector3*, void>)Interop.Resolve("Pine.World.Components.Collider::PineSetSize");
            ColliderGetRadius = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.Collider::PineGetRadius");
            ColliderSetRadius = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.Collider::PineSetRadius");
            ColliderGetHeight = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.Collider::PineGetHeight");
            ColliderSetHeight = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.Collider::PineSetHeight");
            ColliderGetLayer = (delegate* unmanaged<uint, uint>)Interop.Resolve("Pine.World.Components.Collider::PineGetLayer");
            ColliderSetLayer = (delegate* unmanaged<uint, uint, void>)Interop.Resolve("Pine.World.Components.Collider::PineSetLayer");
            ColliderGetLayerMask = (delegate* unmanaged<uint, uint>)Interop.Resolve("Pine.World.Components.Collider::PineGetLayerMask");
            ColliderSetLayerMask = (delegate* unmanaged<uint, uint, void>)Interop.Resolve("Pine.World.Components.Collider::PineSetLayerMask");
            ColliderGetIsTrigger = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Components.Collider::PineGetIsTrigger");
            ColliderSetIsTrigger = (delegate* unmanaged<uint, byte, void>)Interop.Resolve("Pine.World.Components.Collider::PineSetIsTrigger");
            ColliderGetTriggerMask = (delegate* unmanaged<uint, uint>)Interop.Resolve("Pine.World.Components.Collider::PineGetTriggerMask");
            ColliderSetTriggerMask = (delegate* unmanaged<uint, uint, void>)Interop.Resolve("Pine.World.Components.Collider::PineSetTriggerMask");

            AudioSourceGetAudioFile = (delegate* unmanaged<uint, ulong>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetAudioFile");
            AudioSourceSetAudioFile = (delegate* unmanaged<uint, UId, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineSetAudioFile");
            AudioSourcePlay = (delegate* unmanaged<uint, void>)Interop.Resolve("Pine.World.Components.AudioSource::PinePlay");
            AudioSourcePause = (delegate* unmanaged<uint, void>)Interop.Resolve("Pine.World.Components.AudioSource::PinePause");
            AudioSourceStop = (delegate* unmanaged<uint, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineStop");
            AudioSourceGetPlaybackState = (delegate* unmanaged<uint, int>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetPlaybackState");
            AudioSourceIsPlaying = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Components.AudioSource::PineIsPlaying");
            AudioSourceGetPlayOnStart = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetPlayOnStart");
            AudioSourceSetPlayOnStart = (delegate* unmanaged<uint, byte, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineSetPlayOnStart");
            AudioSourceGetLoop = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetLoop");
            AudioSourceSetLoop = (delegate* unmanaged<uint, byte, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineSetLoop");
            AudioSourceGetSpatial = (delegate* unmanaged<uint, byte>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetSpatial");
            AudioSourceSetSpatial = (delegate* unmanaged<uint, byte, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineSetSpatial");
            AudioSourceGetVolume = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetVolume");
            AudioSourceSetVolume = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineSetVolume");
            AudioSourceGetPitch = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetPitch");
            AudioSourceSetPitch = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineSetPitch");
            AudioSourceGetReferenceDistance = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetReferenceDistance");
            AudioSourceSetReferenceDistance = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineSetReferenceDistance");
            AudioSourceGetMaxDistance = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetMaxDistance");
            AudioSourceSetMaxDistance = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineSetMaxDistance");
            AudioSourceGetRolloffFactor = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetRolloffFactor");
            AudioSourceSetRolloffFactor = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineSetRolloffFactor");
            AudioSourceGetPlaybackPosition = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.AudioSource::PineGetPlaybackPosition");
            AudioSourceSetPlaybackPosition = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.AudioSource::PineSetPlaybackPosition");

            AudioListenerGetVolume = (delegate* unmanaged<uint, float>)Interop.Resolve("Pine.World.Components.AudioListener::PineGetVolume");
            AudioListenerSetVolume = (delegate* unmanaged<uint, float, void>)Interop.Resolve("Pine.World.Components.AudioListener::PineSetVolume");

            ScriptGetCSharpScript = (delegate* unmanaged<uint, ulong>)Interop.Resolve("Pine.World.Components.Script::GetScript");
            ScriptGetInstance = (delegate* unmanaged<uint, ulong>)Interop.Resolve("Pine.World.Components.Script::GetScriptInstanceInternal");
        }
    }
}
