#include "Interfaces.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Components/Components.hpp"
#include "mono/metadata/object.h"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/World/Components/CharacterController/CharacterController.hpp"
#include "Pine/World/Components/AudioListener/AudioListener.hpp"
#include "Pine/World/Components/AudioSource/AudioSource.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"

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

    MonoObject* GetModel(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return nullptr;

        auto model = dynamic_cast<Pine::ModelRenderer*>(Pine::Components::GetByInternalId(Pine::ComponentType::ModelRenderer, internalId))->GetModel();

        if (!model)
        {
            return nullptr;
        }

        return mono_gchandle_get_target(model->GetScriptHandle()->Handle);
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

    MonoObject* AudioSourceGetAudioFile(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return nullptr;

        auto audioFile = Pine::Components::GetByInternalId<Pine::AudioSource>(internalId)->GetAudioFile();

        if (!audioFile)
        {
            return nullptr;
        }

        return mono_gchandle_get_target(audioFile->GetScriptHandle()->Handle);
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

    MonoObject* ScriptGetCSharpScript(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return nullptr;

        auto script = dynamic_cast<Pine::ScriptComponent*>(Pine::Components::GetByInternalId(Pine::ComponentType::Script, internalId))->GetScript();
        if (!script) return nullptr;

        return mono_gchandle_get_target(script->GetScriptHandle()->Handle);
    }

    MonoObject* ScriptGetInstance(const std::uint32_t internalId)
    {
        if (std::numeric_limits<std::uint32_t>::max() == internalId) return nullptr;

        auto script = dynamic_cast<Pine::ScriptComponent*>(Pine::Components::GetByInternalId(Pine::ComponentType::Script, internalId));
        if (!script) return nullptr;

        return mono_gchandle_get_target(script->GetScriptObjectHandle()->Handle);
    }

}

void Pine::Script::Interfaces::Component::Setup()
{
    mono_add_internal_call("Pine.World.Component::GetActive", reinterpret_cast<void *>(GetActive));
    mono_add_internal_call("Pine.World.Component::SetActive", reinterpret_cast<void *>(SetActive));

    mono_add_internal_call("Pine.World.Components.ModelRenderer::SetModel", reinterpret_cast<void *>(SetModel));
    mono_add_internal_call("Pine.World.Components.ModelRenderer::GetModel", reinterpret_cast<void *>(GetModel));

    mono_add_internal_call("Pine.World.Components.RigidBody::ApplyForce", reinterpret_cast<void *>(RigidBodyApplyForce));

    mono_add_internal_call("Pine.World.Components.CharacterController::PineMove", reinterpret_cast<void *>(CharacterControllerMove));
    mono_add_internal_call("Pine.World.Components.CharacterController::PineSetPosition", reinterpret_cast<void *>(CharacterControllerSetPosition));
    mono_add_internal_call("Pine.World.Components.CharacterController::PineIsGrounded", reinterpret_cast<void *>(CharacterControllerIsGrounded));
    mono_add_internal_call("Pine.World.Components.CharacterController::PineIsTouchingSides", reinterpret_cast<void *>(CharacterControllerIsTouchingSides));
    mono_add_internal_call("Pine.World.Components.CharacterController::PineGetVelocity", reinterpret_cast<void *>(CharacterControllerGetVelocity));
    mono_add_internal_call("Pine.World.Components.CharacterController::PineSetVerticalVelocity", reinterpret_cast<void *>(CharacterControllerSetVerticalVelocity));
    mono_add_internal_call("Pine.World.Components.CharacterController::PineGetVerticalVelocity", reinterpret_cast<void *>(CharacterControllerGetVerticalVelocity));

    mono_add_internal_call("Pine.World.Components.Transform::GetPosition", reinterpret_cast<void *>(TransformGetPosition));
    mono_add_internal_call("Pine.World.Components.Transform::GetRotation", reinterpret_cast<void *>(TransformGetRotation));
    mono_add_internal_call("Pine.World.Components.Transform::GetScale", reinterpret_cast<void *>(TransformGetScale));
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalPosition", reinterpret_cast<void *>(TransformGetLocalPosition));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalPosition", reinterpret_cast<void *>(TransformSetLocalPosition));
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalRotation", reinterpret_cast<void *>(TransformGetLocalRotation));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalRotation", reinterpret_cast<void *>(TransformSetLocalRotation));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalEulerAngles", reinterpret_cast<void *>(TransformSetLocalEulerAngles));
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalEulerAngles", reinterpret_cast<void *>(TransformGetLocalEulerAngles));
    mono_add_internal_call("Pine.World.Components.Transform::GetLocalScale", reinterpret_cast<void *>(TransformGetLocalScale));
    mono_add_internal_call("Pine.World.Components.Transform::SetLocalScale", reinterpret_cast<void *>(TransformSetLocalScale));
    mono_add_internal_call("Pine.World.Components.Transform::GetUp", reinterpret_cast<void *>(TransformGetUp));
    mono_add_internal_call("Pine.World.Components.Transform::GetRight", reinterpret_cast<void *>(TransformGetRight));
    mono_add_internal_call("Pine.World.Components.Transform::GetForward", reinterpret_cast<void *>(TransformGetForward));

    mono_add_internal_call("Pine.World.Components.Light::PineGetLightType", reinterpret_cast<void *>(LightGetLightType));
    mono_add_internal_call("Pine.World.Components.Light::PineSetLightType", reinterpret_cast<void *>(LightSetLightType));
    mono_add_internal_call("Pine.World.Components.Light::PineGetLightColor", reinterpret_cast<void *>(LightGetLightColor));
    mono_add_internal_call("Pine.World.Components.Light::PineSetLightColor", reinterpret_cast<void *>(LightSetLightColor));
    mono_add_internal_call("Pine.World.Components.Light::PineGetLightIntensity", reinterpret_cast<void *>(LightGetLightIntensity));
    mono_add_internal_call("Pine.World.Components.Light::PineSetLightIntensity", reinterpret_cast<void *>(LightSetLightIntensity));
    mono_add_internal_call("Pine.World.Components.Light::PineGetRange", reinterpret_cast<void *>(LightGetRange));
    mono_add_internal_call("Pine.World.Components.Light::PineSetRange", reinterpret_cast<void *>(LightSetRange));
    mono_add_internal_call("Pine.World.Components.Light::PineGetCastShadows", reinterpret_cast<void *>(LightGetCastShadows));
    mono_add_internal_call("Pine.World.Components.Light::PineSetCastShadows", reinterpret_cast<void *>(LightSetCastShadows));
    mono_add_internal_call("Pine.World.Components.Light::PineGetSpotlightOuterAngle", reinterpret_cast<void *>(LightGetSpotlightOuterAngle));
    mono_add_internal_call("Pine.World.Components.Light::PineSetSpotlightOuterAngle", reinterpret_cast<void *>(LightSetSpotlightOuterAngle));
    mono_add_internal_call("Pine.World.Components.Light::PineGetSpotlightInnerAngle", reinterpret_cast<void *>(LightGetSpotlightInnerAngle));
    mono_add_internal_call("Pine.World.Components.Light::PineSetSpotlightInnerAngle", reinterpret_cast<void *>(LightSetSpotlightInnerAngle));

    mono_add_internal_call("Pine.World.Components.Camera::PineGetCameraType", reinterpret_cast<void *>(CameraGetCameraType));
    mono_add_internal_call("Pine.World.Components.Camera::PineSetCameraType", reinterpret_cast<void *>(CameraSetCameraType));
    mono_add_internal_call("Pine.World.Components.Camera::PineGetNearPlane", reinterpret_cast<void *>(CameraGetNearPlane));
    mono_add_internal_call("Pine.World.Components.Camera::PineSetNearPlane", reinterpret_cast<void *>(CameraSetNearPlane));
    mono_add_internal_call("Pine.World.Components.Camera::PineGetFarPlane", reinterpret_cast<void *>(CameraGetFarPlane));
    mono_add_internal_call("Pine.World.Components.Camera::PineSetFarPlane", reinterpret_cast<void *>(CameraSetFarPlane));
    mono_add_internal_call("Pine.World.Components.Camera::PineGetFieldOfView", reinterpret_cast<void *>(CameraGetFieldOfView));
    mono_add_internal_call("Pine.World.Components.Camera::PineSetFieldOfView", reinterpret_cast<void *>(CameraSetFieldOfView));
    mono_add_internal_call("Pine.World.Components.Camera::PineGetOrthographicSize", reinterpret_cast<void *>(CameraGetOrthographicSize));
    mono_add_internal_call("Pine.World.Components.Camera::PineSetOrthographicSize", reinterpret_cast<void *>(CameraSetOrthographicSize));
    mono_add_internal_call("Pine.World.Components.Camera::PineGetClearColor", reinterpret_cast<void *>(CameraGetClearColor));
    mono_add_internal_call("Pine.World.Components.Camera::PineSetClearColor", reinterpret_cast<void *>(CameraSetClearColor));
    mono_add_internal_call("Pine.World.Components.Camera::PineWorldToScreenPoint", reinterpret_cast<void *>(CameraWorldToScreenPoint));

    mono_add_internal_call("Pine.World.Components.Collider::PineGetColliderType", reinterpret_cast<void *>(ColliderGetColliderType));
    mono_add_internal_call("Pine.World.Components.Collider::PineSetColliderType", reinterpret_cast<void *>(ColliderSetColliderType));
    mono_add_internal_call("Pine.World.Components.Collider::PineGetPosition", reinterpret_cast<void *>(ColliderGetPosition));
    mono_add_internal_call("Pine.World.Components.Collider::PineSetPosition", reinterpret_cast<void *>(ColliderSetPosition));
    mono_add_internal_call("Pine.World.Components.Collider::PineGetSize", reinterpret_cast<void *>(ColliderGetSize));
    mono_add_internal_call("Pine.World.Components.Collider::PineSetSize", reinterpret_cast<void *>(ColliderSetSize));
    mono_add_internal_call("Pine.World.Components.Collider::PineGetRadius", reinterpret_cast<void *>(ColliderGetRadius));
    mono_add_internal_call("Pine.World.Components.Collider::PineSetRadius", reinterpret_cast<void *>(ColliderSetRadius));
    mono_add_internal_call("Pine.World.Components.Collider::PineGetHeight", reinterpret_cast<void *>(ColliderGetHeight));
    mono_add_internal_call("Pine.World.Components.Collider::PineSetHeight", reinterpret_cast<void *>(ColliderSetHeight));
    mono_add_internal_call("Pine.World.Components.Collider::PineGetLayer", reinterpret_cast<void *>(ColliderGetLayer));
    mono_add_internal_call("Pine.World.Components.Collider::PineSetLayer", reinterpret_cast<void *>(ColliderSetLayer));
    mono_add_internal_call("Pine.World.Components.Collider::PineGetLayerMask", reinterpret_cast<void *>(ColliderGetLayerMask));
    mono_add_internal_call("Pine.World.Components.Collider::PineSetLayerMask", reinterpret_cast<void *>(ColliderSetLayerMask));
    mono_add_internal_call("Pine.World.Components.Collider::PineGetIsTrigger", reinterpret_cast<void *>(ColliderGetIsTrigger));
    mono_add_internal_call("Pine.World.Components.Collider::PineSetIsTrigger", reinterpret_cast<void *>(ColliderSetIsTrigger));
    mono_add_internal_call("Pine.World.Components.Collider::PineGetTriggerMask", reinterpret_cast<void *>(ColliderGetTriggerMask));
    mono_add_internal_call("Pine.World.Components.Collider::PineSetTriggerMask", reinterpret_cast<void *>(ColliderSetTriggerMask));

    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetAudioFile", reinterpret_cast<void *>(AudioSourceGetAudioFile));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineSetAudioFile", reinterpret_cast<void *>(AudioSourceSetAudioFile));
    mono_add_internal_call("Pine.World.Components.AudioSource::PinePlay", reinterpret_cast<void *>(AudioSourcePlay));
    mono_add_internal_call("Pine.World.Components.AudioSource::PinePause", reinterpret_cast<void *>(AudioSourcePause));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineStop", reinterpret_cast<void *>(AudioSourceStop));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetPlaybackState", reinterpret_cast<void *>(AudioSourceGetPlaybackState));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineIsPlaying", reinterpret_cast<void *>(AudioSourceIsPlaying));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetPlayOnStart", reinterpret_cast<void *>(AudioSourceGetPlayOnStart));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineSetPlayOnStart", reinterpret_cast<void *>(AudioSourceSetPlayOnStart));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetLoop", reinterpret_cast<void *>(AudioSourceGetLoop));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineSetLoop", reinterpret_cast<void *>(AudioSourceSetLoop));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetSpatial", reinterpret_cast<void *>(AudioSourceGetSpatial));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineSetSpatial", reinterpret_cast<void *>(AudioSourceSetSpatial));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetVolume", reinterpret_cast<void *>(AudioSourceGetVolume));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineSetVolume", reinterpret_cast<void *>(AudioSourceSetVolume));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetPitch", reinterpret_cast<void *>(AudioSourceGetPitch));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineSetPitch", reinterpret_cast<void *>(AudioSourceSetPitch));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetReferenceDistance", reinterpret_cast<void *>(AudioSourceGetReferenceDistance));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineSetReferenceDistance", reinterpret_cast<void *>(AudioSourceSetReferenceDistance));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetMaxDistance", reinterpret_cast<void *>(AudioSourceGetMaxDistance));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineSetMaxDistance", reinterpret_cast<void *>(AudioSourceSetMaxDistance));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetRolloffFactor", reinterpret_cast<void *>(AudioSourceGetRolloffFactor));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineSetRolloffFactor", reinterpret_cast<void *>(AudioSourceSetRolloffFactor));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineGetPlaybackPosition", reinterpret_cast<void *>(AudioSourceGetPlaybackPosition));
    mono_add_internal_call("Pine.World.Components.AudioSource::PineSetPlaybackPosition", reinterpret_cast<void *>(AudioSourceSetPlaybackPosition));

    mono_add_internal_call("Pine.World.Components.AudioListener::PineGetVolume", reinterpret_cast<void *>(AudioListenerGetVolume));
    mono_add_internal_call("Pine.World.Components.AudioListener::PineSetVolume", reinterpret_cast<void *>(AudioListenerSetVolume));

    mono_add_internal_call("Pine.World.Components.Script::GetScript", reinterpret_cast<void *>(ScriptGetCSharpScript));
    mono_add_internal_call("Pine.World.Components.Script::GetScriptInstanceInternal", reinterpret_cast<void *>(ScriptGetInstance));
}