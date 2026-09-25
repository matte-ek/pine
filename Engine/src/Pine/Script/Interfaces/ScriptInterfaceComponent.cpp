#include "Interfaces.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/World/Components/CharacterController/CharacterController.hpp"
#include "Pine/World/Components/AudioListener/AudioListener.hpp"
#include "Pine/World/Components/AudioSource/AudioSource.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/Script/Bindings/Bindings.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

namespace
{
    bool GetActive(const std::uint32_t internalId, int type)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;
        return Pine::Components::GetByInternalId(static_cast<Pine::ComponentType>(type), internalId)->GetActive();
    }

    void SetActive(const std::uint32_t internalId, int type, const bool active)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;
        Pine::Components::GetByInternalId(static_cast<Pine::ComponentType>(type), internalId)->SetActive(active);
    }

    // -----------------------------------------------------

    void SetModel(const std::uint32_t internalId, Pine::UId assetId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        dynamic_cast<Pine::ModelRenderer*>(Pine::Components::GetByInternalId(Pine::ComponentType::ModelRenderer, internalId))->SetModel(
            dynamic_cast<Pine::Model*>(Pine::Assets::GetAssetByUId(assetId))
        );
    }

    std::uint64_t GetModel(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        const auto model = dynamic_cast<Pine::ModelRenderer*>(Pine::Components::GetByInternalId(Pine::ComponentType::ModelRenderer, internalId))->GetModel();

        if (!model)
        {
            return 0;
        }

        return model->GetScriptHandle()->Id;
    }

    bool ModelRendererGetCastShadows(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;

        return Pine::Components::GetByInternalId<Pine::ModelRenderer>(internalId)->GetCastShadows();
    }

    void ModelRendererSetCastShadows(const std::uint32_t internalId, const bool castShadows)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::ModelRenderer>(internalId)->SetCastShadows(castShadows);
    }

    bool ModelRendererGetReceiveShadows(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;

        return Pine::Components::GetByInternalId<Pine::ModelRenderer>(internalId)->GetReceiveShadows();
    }

    void ModelRendererSetReceiveShadows(const std::uint32_t internalId, const bool receiveShadows)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::ModelRenderer>(internalId)->SetReceiveShadows(receiveShadows);
    }

    // -----------------------------------------------------

    void TransformGetPosition(const std::uint32_t internalId, Pine::Vector3f *position)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *position = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetPosition();
    }

    void TransformGetRotation(const std::uint32_t internalId, Pine::Quaternion *rotation)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *rotation = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetRotation();
    }

    void TransformGetScale(const std::uint32_t internalId, Pine::Vector3f *scale)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *scale = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetScale();
    }

    void TransformGetLocalPosition(const std::uint32_t internalId, Pine::Vector3f *position)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *position = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetLocalPosition();
    }

    void TransformSetLocalPosition(const std::uint32_t internalId, Pine::Vector3f *position)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->SetLocalPosition(*position);
    }

    void TransformGetLocalRotation(const std::uint32_t internalId, Pine::Quaternion *rotation)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *rotation = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetLocalRotation();
    }

    void TransformSetLocalRotation(const std::uint32_t internalId, Pine::Quaternion *rotation)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->SetLocalRotation(*rotation);
    }

    void TransformGetLocalEulerAngles(const std::uint32_t internalId, Pine::Vector3f *rotation)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *rotation = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetEulerAngles();
    }

    void TransformSetLocalEulerAngles(const std::uint32_t internalId, Pine::Vector3f *rotation)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->SetEulerAngles(*rotation);
    }

    void TransformGetLocalScale(const std::uint32_t internalId, Pine::Vector3f *scale)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *scale = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetLocalScale();
    }

    void TransformSetLocalScale(const std::uint32_t internalId, Pine::Vector3f *scale)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Transform>(internalId)->SetLocalScale(*scale);
    }

    void TransformGetUp(const std::uint32_t internalId, Pine::Vector3f *up)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *up = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetUp();
    }

    void TransformGetRight(const std::uint32_t internalId, Pine::Vector3f *right)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *right = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetRight();
    }

    void TransformGetForward(const std::uint32_t internalId, Pine::Vector3f *forward)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *forward = Pine::Components::GetByInternalId<Pine::Transform>(internalId)->GetForward();
    }

    // -----------------------------------------------------

    void RigidBodyApplyForce(const std::uint32_t internalId, const Pine::Vector3f *force, const physx::PxForceMode::Enum mode)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::RigidBody>(internalId)->ApplyForce(*force, mode);
    }

    // -----------------------------------------------------

    void CharacterControllerMove(const std::uint32_t internalId, const Pine::Vector3f* motion)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::CharacterController>(internalId)->Move(*motion);
    }

    void CharacterControllerSetPosition(const std::uint32_t internalId, const Pine::Vector3f* position)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::CharacterController>(internalId)->SetPosition(*position);
    }

    bool CharacterControllerIsGrounded(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;

        return Pine::Components::GetByInternalId<Pine::CharacterController>(internalId)->IsGrounded();
    }

    bool CharacterControllerIsTouchingSides(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;

        return Pine::Components::GetByInternalId<Pine::CharacterController>(internalId)->IsTouchingSides();
    }

    void CharacterControllerGetVelocity(const std::uint32_t internalId, Pine::Vector3f* velocity)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *velocity = Pine::Components::GetByInternalId<Pine::CharacterController>(internalId)->GetVelocity();
    }

    void CharacterControllerSetVerticalVelocity(const std::uint32_t internalId, const float verticalVelocity)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::CharacterController>(internalId)->SetVerticalVelocity(verticalVelocity);
    }

    float CharacterControllerGetVerticalVelocity(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::CharacterController>(internalId)->GetVerticalVelocity();
    }

    // -----------------------------------------------------

    int LightGetLightType(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        return static_cast<int>(Pine::Components::GetByInternalId<Pine::Light>(internalId)->GetLightType());
    }

    void LightSetLightType(const std::uint32_t internalId, const int type)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Light>(internalId)->SetLightType(static_cast<Pine::LightType>(type));
    }

    void LightGetLightColor(const std::uint32_t internalId, Pine::Vector3f* color)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *color = Pine::Components::GetByInternalId<Pine::Light>(internalId)->GetLightColor();
    }

    void LightSetLightColor(const std::uint32_t internalId, const Pine::Vector3f* color)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Light>(internalId)->SetLightColor(*color);
    }

    float LightGetLightIntensity(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::Light>(internalId)->GetLightIntensity();
    }

    void LightSetLightIntensity(const std::uint32_t internalId, const float intensity)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Light>(internalId)->SetLightIntensity(intensity);
    }

    float LightGetRange(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::Light>(internalId)->GetRange();
    }

    void LightSetRange(const std::uint32_t internalId, const float range)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Light>(internalId)->SetRange(range);
    }

    bool LightGetCastShadows(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;

        return Pine::Components::GetByInternalId<Pine::Light>(internalId)->GetCastShadows();
    }

    void LightSetCastShadows(const std::uint32_t internalId, const bool castShadows)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Light>(internalId)->SetCastShadows(castShadows);
    }

    float LightGetSpotlightOuterAngle(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::Light>(internalId)->GetSpotlightOuterAngle();
    }

    void LightSetSpotlightOuterAngle(const std::uint32_t internalId, const float degrees)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Light>(internalId)->SetSpotlightOuterAngle(degrees);
    }

    float LightGetSpotlightInnerAngle(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::Light>(internalId)->GetSpotlightInnerAngle();
    }

    void LightSetSpotlightInnerAngle(const std::uint32_t internalId, const float degrees)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Light>(internalId)->SetSpotlightInnerAngle(degrees);
    }

    // -----------------------------------------------------

    int CameraGetCameraType(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        return static_cast<int>(Pine::Components::GetByInternalId<Pine::Camera>(internalId)->GetCameraType());
    }

    void CameraSetCameraType(const std::uint32_t internalId, const int type)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Camera>(internalId)->SetCameraType(static_cast<Pine::CameraType>(type));
    }

    float CameraGetNearPlane(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::Camera>(internalId)->GetNearPlane();
    }

    void CameraSetNearPlane(const std::uint32_t internalId, const float value)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Camera>(internalId)->SetNearPlane(value);
    }

    float CameraGetFarPlane(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::Camera>(internalId)->GetFarPlane();
    }

    void CameraSetFarPlane(const std::uint32_t internalId, const float value)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Camera>(internalId)->SetFarPlane(value);
    }

    float CameraGetFieldOfView(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::Camera>(internalId)->GetFieldOfView();
    }

    void CameraSetFieldOfView(const std::uint32_t internalId, const float value)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Camera>(internalId)->SetFieldOfView(value);
    }

    float CameraGetOrthographicSize(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::Camera>(internalId)->GetOrthographicSize();
    }

    void CameraSetOrthographicSize(const std::uint32_t internalId, const float value)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Camera>(internalId)->SetOrthographicSize(value);
    }

    void CameraGetClearColor(const std::uint32_t internalId, Pine::Vector4f* color)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *color = Pine::Components::GetByInternalId<Pine::Camera>(internalId)->GetClearColor();
    }

    void CameraSetClearColor(const std::uint32_t internalId, const Pine::Vector4f* color)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Camera>(internalId)->SetClearColor(*color);
    }

    void CameraWorldToScreenPoint(const std::uint32_t internalId, const Pine::Vector3f* position, Pine::Vector3f* screenPoint)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *screenPoint = Pine::Components::GetByInternalId<Pine::Camera>(internalId)->WorldToScreenPoint(*position);
    }

    // -----------------------------------------------------

    int ColliderGetColliderType(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        return static_cast<int>(Pine::Components::GetByInternalId<Pine::Collider>(internalId)->GetColliderType());
    }

    void ColliderSetColliderType(const std::uint32_t internalId, const int type)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Collider>(internalId)->SetColliderType(static_cast<Pine::ColliderType>(type));
    }

    void ColliderGetPosition(const std::uint32_t internalId, Pine::Vector3f* position)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *position = Pine::Components::GetByInternalId<Pine::Collider>(internalId)->GetPosition();
    }

    void ColliderSetPosition(const std::uint32_t internalId, const Pine::Vector3f* position)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Collider>(internalId)->SetPosition(*position);
    }

    void ColliderGetSize(const std::uint32_t internalId, Pine::Vector3f* size)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        *size = Pine::Components::GetByInternalId<Pine::Collider>(internalId)->GetSize();
    }

    void ColliderSetSize(const std::uint32_t internalId, const Pine::Vector3f* size)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Collider>(internalId)->SetSize(*size);
    }

    float ColliderGetRadius(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::Collider>(internalId)->GetRadius();
    }

    void ColliderSetRadius(const std::uint32_t internalId, const float radius)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Collider>(internalId)->SetRadius(radius);
    }

    float ColliderGetHeight(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::Collider>(internalId)->GetHeight();
    }

    void ColliderSetHeight(const std::uint32_t internalId, const float height)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Collider>(internalId)->SetHeight(height);
    }

    std::uint32_t ColliderGetLayer(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        return Pine::Components::GetByInternalId<Pine::Collider>(internalId)->GetLayer();
    }

    void ColliderSetLayer(const std::uint32_t internalId, const std::uint32_t layer)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Collider>(internalId)->SetLayer(layer);
    }

    std::uint32_t ColliderGetLayerMask(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        return Pine::Components::GetByInternalId<Pine::Collider>(internalId)->GetLayerMask();
    }

    void ColliderSetLayerMask(const std::uint32_t internalId, const std::uint32_t layerMask)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Collider>(internalId)->SetLayerMask(layerMask);
    }

    bool ColliderGetIsTrigger(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;

        return Pine::Components::GetByInternalId<Pine::Collider>(internalId)->IsTrigger();
    }

    void ColliderSetIsTrigger(const std::uint32_t internalId, const bool isTrigger)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Collider>(internalId)->SetIsTrigger(isTrigger);
    }

    std::uint32_t ColliderGetTriggerMask(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        return Pine::Components::GetByInternalId<Pine::Collider>(internalId)->GetTriggerMask();
    }

    void ColliderSetTriggerMask(const std::uint32_t internalId, const std::uint32_t mask)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::Collider>(internalId)->SetTriggerMask(mask);
    }

    // -----------------------------------------------------

    std::uint64_t AudioSourceGetAudioFile(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        const auto audioFile = Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetAudioFile();

        if (!audioFile)
        {
            return 0;
        }

        return audioFile->GetScriptHandle()->Id;
    }

    void AudioSourceSetAudioFile(const std::uint32_t internalId, Pine::UId assetId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        // An unknown id resolves to nullptr, which is how C# clears the clip by assigning null.
        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->SetAudioFile(
            dynamic_cast<Pine::AudioFile*>(Pine::Assets::GetAssetByUId(assetId))
        );
    }

    void AudioSourcePlay(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->Play();
    }

    void AudioSourcePause(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->Pause();
    }

    void AudioSourceStop(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->Stop();
    }

    int AudioSourceGetPlaybackState(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return static_cast<int>(Pine::Audio::PlaybackState::Stopped);

        return static_cast<int>(Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetPlaybackState());
    }

    bool AudioSourceIsPlaying(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;

        return Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->IsPlaying();
    }

    bool AudioSourceGetPlayOnStart(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;

        return Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetPlayOnStart();
    }

    void AudioSourceSetPlayOnStart(const std::uint32_t internalId, const bool playOnStart)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->SetPlayOnStart(playOnStart);
    }

    bool AudioSourceGetLoop(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;

        return Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetLoop();
    }

    void AudioSourceSetLoop(const std::uint32_t internalId, const bool loop)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->SetLoop(loop);
    }

    bool AudioSourceGetSpatial(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return false;

        return Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetSpatial();
    }

    void AudioSourceSetSpatial(const std::uint32_t internalId, const bool spatial)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->SetSpatial(spatial);
    }

    float AudioSourceGetVolume(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetVolume();
    }

    void AudioSourceSetVolume(const std::uint32_t internalId, const float volume)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->SetVolume(volume);
    }

    float AudioSourceGetPitch(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetPitch();
    }

    void AudioSourceSetPitch(const std::uint32_t internalId, const float pitch)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->SetPitch(pitch);
    }

    float AudioSourceGetReferenceDistance(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetReferenceDistance();
    }

    void AudioSourceSetReferenceDistance(const std::uint32_t internalId, const float distance)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->SetReferenceDistance(distance);
    }

    float AudioSourceGetMaxDistance(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetMaxDistance();
    }

    void AudioSourceSetMaxDistance(const std::uint32_t internalId, const float distance)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->SetMaxDistance(distance);
    }

    float AudioSourceGetRolloffFactor(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetRolloffFactor();
    }

    void AudioSourceSetRolloffFactor(const std::uint32_t internalId, const float factor)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->SetRolloffFactor(factor);
    }

    float AudioSourceGetPlaybackPosition(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetPlaybackPosition();
    }

    void AudioSourceSetPlaybackPosition(const std::uint32_t internalId, const float seconds)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->SetPlaybackPosition(seconds);
    }

    // -----------------------------------------------------

    float AudioListenerGetVolume(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0.f;

        return Pine::Components::GetByInternalId<Pine::AudioListener>(internalId)->GetVolume();
    }

    void AudioListenerSetVolume(const std::uint32_t internalId, const float volume)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return;

        Pine::Components::GetByInternalId<Pine::AudioListener>(internalId)->SetVolume(volume);
    }

    // -----------------------------------------------------

    std::uint64_t ScriptGetCSharpScript(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        const auto script = dynamic_cast<Pine::ScriptComponent*>(Pine::Components::GetByInternalId(Pine::ComponentType::Script, internalId))->GetScript();
        if (!script) return 0;

        return script->GetScriptHandle()->Id;
    }

    // The one binding that hands back an object of the game's own making rather than one of
    // Pine.dll's, so the only handle here that belongs to the collectible load context.
    std::uint64_t ScriptGetInstance(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return 0;

        const auto script = dynamic_cast<Pine::ScriptComponent*>(Pine::Components::GetByInternalId(Pine::ComponentType::Script, internalId));
        if (!script) return 0;

        return script->GetScriptObjectHandle()->Id;
    }

}

void Pine::Script::Interfaces::Component::Setup()
{
    Bindings::Register("Pine.World.Component::GetActive", GetActive);
    Bindings::Register("Pine.World.Component::SetActive", SetActive);

    Bindings::Register("Pine.World.Components.ModelRenderer::SetModel", SetModel);
    Bindings::Register("Pine.World.Components.ModelRenderer::GetModel", GetModel);
    Bindings::Register("Pine.World.Components.ModelRenderer::PineGetCastShadows", ModelRendererGetCastShadows);
    Bindings::Register("Pine.World.Components.ModelRenderer::PineSetCastShadows", ModelRendererSetCastShadows);
    Bindings::Register("Pine.World.Components.ModelRenderer::PineGetReceiveShadows", ModelRendererGetReceiveShadows);
    Bindings::Register("Pine.World.Components.ModelRenderer::PineSetReceiveShadows", ModelRendererSetReceiveShadows);

    Bindings::Register("Pine.World.Components.RigidBody::ApplyForce", RigidBodyApplyForce);

    Bindings::Register("Pine.World.Components.CharacterController::PineMove", CharacterControllerMove);
    Bindings::Register("Pine.World.Components.CharacterController::PineSetPosition", CharacterControllerSetPosition);
    Bindings::Register("Pine.World.Components.CharacterController::PineIsGrounded", CharacterControllerIsGrounded);
    Bindings::Register("Pine.World.Components.CharacterController::PineIsTouchingSides", CharacterControllerIsTouchingSides);
    Bindings::Register("Pine.World.Components.CharacterController::PineGetVelocity", CharacterControllerGetVelocity);
    Bindings::Register("Pine.World.Components.CharacterController::PineSetVerticalVelocity", CharacterControllerSetVerticalVelocity);
    Bindings::Register("Pine.World.Components.CharacterController::PineGetVerticalVelocity", CharacterControllerGetVerticalVelocity);

    Bindings::Register("Pine.World.Components.Transform::GetPosition", TransformGetPosition);
    Bindings::Register("Pine.World.Components.Transform::GetRotation", TransformGetRotation);
    Bindings::Register("Pine.World.Components.Transform::GetScale", TransformGetScale);
    Bindings::Register("Pine.World.Components.Transform::GetLocalPosition", TransformGetLocalPosition);
    Bindings::Register("Pine.World.Components.Transform::SetLocalPosition", TransformSetLocalPosition);
    Bindings::Register("Pine.World.Components.Transform::GetLocalRotation", TransformGetLocalRotation);
    Bindings::Register("Pine.World.Components.Transform::SetLocalRotation", TransformSetLocalRotation);
    Bindings::Register("Pine.World.Components.Transform::SetLocalEulerAngles", TransformSetLocalEulerAngles);
    Bindings::Register("Pine.World.Components.Transform::GetLocalEulerAngles", TransformGetLocalEulerAngles);
    Bindings::Register("Pine.World.Components.Transform::GetLocalScale", TransformGetLocalScale);
    Bindings::Register("Pine.World.Components.Transform::SetLocalScale", TransformSetLocalScale);
    Bindings::Register("Pine.World.Components.Transform::GetUp", TransformGetUp);
    Bindings::Register("Pine.World.Components.Transform::GetRight", TransformGetRight);
    Bindings::Register("Pine.World.Components.Transform::GetForward", TransformGetForward);

    Bindings::Register("Pine.World.Components.Light::PineGetLightType", LightGetLightType);
    Bindings::Register("Pine.World.Components.Light::PineSetLightType", LightSetLightType);
    Bindings::Register("Pine.World.Components.Light::PineGetLightColor", LightGetLightColor);
    Bindings::Register("Pine.World.Components.Light::PineSetLightColor", LightSetLightColor);
    Bindings::Register("Pine.World.Components.Light::PineGetLightIntensity", LightGetLightIntensity);
    Bindings::Register("Pine.World.Components.Light::PineSetLightIntensity", LightSetLightIntensity);
    Bindings::Register("Pine.World.Components.Light::PineGetRange", LightGetRange);
    Bindings::Register("Pine.World.Components.Light::PineSetRange", LightSetRange);
    Bindings::Register("Pine.World.Components.Light::PineGetCastShadows", LightGetCastShadows);
    Bindings::Register("Pine.World.Components.Light::PineSetCastShadows", LightSetCastShadows);
    Bindings::Register("Pine.World.Components.Light::PineGetSpotlightOuterAngle", LightGetSpotlightOuterAngle);
    Bindings::Register("Pine.World.Components.Light::PineSetSpotlightOuterAngle", LightSetSpotlightOuterAngle);
    Bindings::Register("Pine.World.Components.Light::PineGetSpotlightInnerAngle", LightGetSpotlightInnerAngle);
    Bindings::Register("Pine.World.Components.Light::PineSetSpotlightInnerAngle", LightSetSpotlightInnerAngle);

    Bindings::Register("Pine.World.Components.Camera::PineGetCameraType", CameraGetCameraType);
    Bindings::Register("Pine.World.Components.Camera::PineSetCameraType", CameraSetCameraType);
    Bindings::Register("Pine.World.Components.Camera::PineGetNearPlane", CameraGetNearPlane);
    Bindings::Register("Pine.World.Components.Camera::PineSetNearPlane", CameraSetNearPlane);
    Bindings::Register("Pine.World.Components.Camera::PineGetFarPlane", CameraGetFarPlane);
    Bindings::Register("Pine.World.Components.Camera::PineSetFarPlane", CameraSetFarPlane);
    Bindings::Register("Pine.World.Components.Camera::PineGetFieldOfView", CameraGetFieldOfView);
    Bindings::Register("Pine.World.Components.Camera::PineSetFieldOfView", CameraSetFieldOfView);
    Bindings::Register("Pine.World.Components.Camera::PineGetOrthographicSize", CameraGetOrthographicSize);
    Bindings::Register("Pine.World.Components.Camera::PineSetOrthographicSize", CameraSetOrthographicSize);
    Bindings::Register("Pine.World.Components.Camera::PineGetClearColor", CameraGetClearColor);
    Bindings::Register("Pine.World.Components.Camera::PineSetClearColor", CameraSetClearColor);
    Bindings::Register("Pine.World.Components.Camera::PineWorldToScreenPoint", CameraWorldToScreenPoint);

    Bindings::Register("Pine.World.Components.Collider::PineGetColliderType", ColliderGetColliderType);
    Bindings::Register("Pine.World.Components.Collider::PineSetColliderType", ColliderSetColliderType);
    Bindings::Register("Pine.World.Components.Collider::PineGetPosition", ColliderGetPosition);
    Bindings::Register("Pine.World.Components.Collider::PineSetPosition", ColliderSetPosition);
    Bindings::Register("Pine.World.Components.Collider::PineGetSize", ColliderGetSize);
    Bindings::Register("Pine.World.Components.Collider::PineSetSize", ColliderSetSize);
    Bindings::Register("Pine.World.Components.Collider::PineGetRadius", ColliderGetRadius);
    Bindings::Register("Pine.World.Components.Collider::PineSetRadius", ColliderSetRadius);
    Bindings::Register("Pine.World.Components.Collider::PineGetHeight", ColliderGetHeight);
    Bindings::Register("Pine.World.Components.Collider::PineSetHeight", ColliderSetHeight);
    Bindings::Register("Pine.World.Components.Collider::PineGetLayer", ColliderGetLayer);
    Bindings::Register("Pine.World.Components.Collider::PineSetLayer", ColliderSetLayer);
    Bindings::Register("Pine.World.Components.Collider::PineGetLayerMask", ColliderGetLayerMask);
    Bindings::Register("Pine.World.Components.Collider::PineSetLayerMask", ColliderSetLayerMask);
    Bindings::Register("Pine.World.Components.Collider::PineGetIsTrigger", ColliderGetIsTrigger);
    Bindings::Register("Pine.World.Components.Collider::PineSetIsTrigger", ColliderSetIsTrigger);
    Bindings::Register("Pine.World.Components.Collider::PineGetTriggerMask", ColliderGetTriggerMask);
    Bindings::Register("Pine.World.Components.Collider::PineSetTriggerMask", ColliderSetTriggerMask);

    Bindings::Register("Pine.World.Components.AudioSource::PineGetAudioFile", AudioSourceGetAudioFile);
    Bindings::Register("Pine.World.Components.AudioSource::PineSetAudioFile", AudioSourceSetAudioFile);
    Bindings::Register("Pine.World.Components.AudioSource::PinePlay", AudioSourcePlay);
    Bindings::Register("Pine.World.Components.AudioSource::PinePause", AudioSourcePause);
    Bindings::Register("Pine.World.Components.AudioSource::PineStop", AudioSourceStop);
    Bindings::Register("Pine.World.Components.AudioSource::PineGetPlaybackState", AudioSourceGetPlaybackState);
    Bindings::Register("Pine.World.Components.AudioSource::PineIsPlaying", AudioSourceIsPlaying);
    Bindings::Register("Pine.World.Components.AudioSource::PineGetPlayOnStart", AudioSourceGetPlayOnStart);
    Bindings::Register("Pine.World.Components.AudioSource::PineSetPlayOnStart", AudioSourceSetPlayOnStart);
    Bindings::Register("Pine.World.Components.AudioSource::PineGetLoop", AudioSourceGetLoop);
    Bindings::Register("Pine.World.Components.AudioSource::PineSetLoop", AudioSourceSetLoop);
    Bindings::Register("Pine.World.Components.AudioSource::PineGetSpatial", AudioSourceGetSpatial);
    Bindings::Register("Pine.World.Components.AudioSource::PineSetSpatial", AudioSourceSetSpatial);
    Bindings::Register("Pine.World.Components.AudioSource::PineGetVolume", AudioSourceGetVolume);
    Bindings::Register("Pine.World.Components.AudioSource::PineSetVolume", AudioSourceSetVolume);
    Bindings::Register("Pine.World.Components.AudioSource::PineGetPitch", AudioSourceGetPitch);
    Bindings::Register("Pine.World.Components.AudioSource::PineSetPitch", AudioSourceSetPitch);
    Bindings::Register("Pine.World.Components.AudioSource::PineGetReferenceDistance", AudioSourceGetReferenceDistance);
    Bindings::Register("Pine.World.Components.AudioSource::PineSetReferenceDistance", AudioSourceSetReferenceDistance);
    Bindings::Register("Pine.World.Components.AudioSource::PineGetMaxDistance", AudioSourceGetMaxDistance);
    Bindings::Register("Pine.World.Components.AudioSource::PineSetMaxDistance", AudioSourceSetMaxDistance);
    Bindings::Register("Pine.World.Components.AudioSource::PineGetRolloffFactor", AudioSourceGetRolloffFactor);
    Bindings::Register("Pine.World.Components.AudioSource::PineSetRolloffFactor", AudioSourceSetRolloffFactor);
    Bindings::Register("Pine.World.Components.AudioSource::PineGetPlaybackPosition", AudioSourceGetPlaybackPosition);
    Bindings::Register("Pine.World.Components.AudioSource::PineSetPlaybackPosition", AudioSourceSetPlaybackPosition);

    Bindings::Register("Pine.World.Components.AudioListener::PineGetVolume", AudioListenerGetVolume);
    Bindings::Register("Pine.World.Components.AudioListener::PineSetVolume", AudioListenerSetVolume);

    Bindings::Register("Pine.World.Components.Script::GetScript", ScriptGetCSharpScript);
    Bindings::Register("Pine.World.Components.Script::GetScriptInstanceInternal", ScriptGetInstance);
}