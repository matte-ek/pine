#include "AssetPreview.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Core/Math/ViewFit/ViewFit.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Light/Light.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    // Leaves a little air around the subject instead of having it touch the edge of the image.
    constexpr float SubjectViewPadding = 1.15f;

    // Key light sits over the camera's shoulder, so the subject is lit consistently no matter where
    // the view angle ends up.
    constexpr Pine::Vector2f KeyLightAngleOffset = Pine::Vector2f(25.f, 15.f);

    Pine::Vector3f DirectionFromAngles(const Pine::Vector2f &angles)
    {
        const float yaw = glm::radians(angles.x);
        const float pitch = glm::radians(glm::clamp(angles.y, -89.f, 89.f));

        return {
            glm::cos(pitch) * glm::sin(yaw),
            glm::sin(pitch),
            glm::cos(pitch) * glm::cos(yaw)
        };
    }

    // Aims a transform at a point from a given direction, i.e. the direction is where it sits
    // relative to the target, not where it looks.
    void PlaceLookingAt(Pine::Transform *transform, const Pine::Vector3f &target, const Pine::Vector3f &direction, float distance)
    {
        transform->SetLocalPosition(target + direction * distance);
        transform->SetLocalRotation(glm::quatLookAt(-direction, Pine::Vector3f(0.f, 1.f, 0.f)));
    }

    // The subject itself is never moved or scaled to fit - the camera is what moves. Scaling the
    // subject would misreport anything whose look depends on world scale, and would do nothing
    // about the depth range, which is the half that decides whether a very large or very small
    // model is visible at all.
    void SetupView(Pine::Camera *camera, Pine::Light *light, const Pine::Vector3f &boundsMin, const Pine::Vector3f &boundsMax, const Pine::Vector2f &viewAngle, float aspectRatio)
    {
        const auto fit = Pine::ViewFit::FromBounds(boundsMin, boundsMax, camera->GetFieldOfView(), aspectRatio, SubjectViewPadding);

        PlaceLookingAt(camera->GetParent()->GetTransform(), fit.Center, DirectionFromAngles(viewAngle), fit.Distance);

        camera->SetOverrideAspectRatio(aspectRatio);
        camera->SetNearPlane(fit.NearPlane);
        camera->SetFarPlane(fit.FarPlane);
        camera->OnRender(0.f);

        // Only the rotation of a directional light matters, but it still needs a sane position for
        // the transform to resolve.
        PlaceLookingAt(light->GetParent()->GetTransform(), fit.Center, DirectionFromAngles(viewAngle + KeyLightAngleOffset), fit.Distance);
    }

    Pine::Model *GetPreviewSphere()
    {
        static auto sphereModel = Pine::Assets::Get<Pine::Model>("editor/models/sphere");

        return sphereModel;
    }

    // What the preview is framed on: a material is shown on the editor's sphere, a model on itself.
    Pine::Model *GetSubjectModel(Pine::Asset *asset)
    {
        if (asset == nullptr)
        {
            return nullptr;
        }

        if (asset->GetType() == Pine::AssetType::Material)
        {
            return GetPreviewSphere();
        }

        if (asset->GetType() == Pine::AssetType::Model)
        {
            return dynamic_cast<Pine::Model *>(asset);
        }

        return nullptr;
    }

    // Every subject renders at the origin with an identity transform, since the camera is what was
    // fitted to it.
    const Pine::Matrix4f &GetSubjectTransform()
    {
        static Pine::Entity *subjectEntity = nullptr;

        if (subjectEntity == nullptr)
        {
            subjectEntity = new Pine::Entity(Pine::UId::Empty());

            subjectEntity->AddComponent(new Pine::Transform());
        }

        return subjectEntity->GetTransform()->GetTransformationMatrix();
    }

    void RenderSubject(Pine::Asset *asset, Pine::Model *model)
    {
        if (asset->GetType() == Pine::AssetType::Material)
        {
            Pine::Renderer3D::PrepareMesh(model->GetMeshes()[0], dynamic_cast<Pine::Material *>(asset));
            Pine::Renderer3D::RenderMesh(GetSubjectTransform());

            return;
        }

        for (const auto &mesh: model->GetMeshes())
        {
            Pine::Renderer3D::PrepareMesh(mesh);
            Pine::Renderer3D::RenderMesh(GetSubjectTransform());
        }
    }
}

bool Editor::AssetPreview::Supports(const Pine::Asset *asset)
{
    if (asset == nullptr)
    {
        return false;
    }

    switch (asset->GetType())
    {
        case Pine::AssetType::Material:
            return true;
        case Pine::AssetType::Model:
            return true;
        default:
            return false;
    }
}

bool Editor::AssetPreview::Render(Pine::Asset *asset, const Options &options, Pine::Graphics::IFrameBuffer *target)
{
    static Pine::Entity *lightEntity = nullptr;
    static Pine::Entity *cameraEntity = nullptr;

    if (!Supports(asset) || target == nullptr)
    {
        return false;
    }

    const auto model = GetSubjectModel(asset);

    if (model == nullptr || model->GetMeshes().empty())
    {
        return false;
    }

    if (options.Size.x < 1 || options.Size.y < 1 ||
        options.Size.x > target->GetSize().x || options.Size.y > target->GetSize().y)
    {
        return false;
    }

    if (lightEntity == nullptr)
    {
        lightEntity = new Pine::Entity(Pine::UId::Empty());

        lightEntity->AddComponent(new Pine::Transform());
        lightEntity->AddComponent(new Pine::Light());
    }

    lightEntity->GetComponent<Pine::Light>()->SetLightIntensity(options.LightIntensity);

    if (cameraEntity == nullptr)
    {
        cameraEntity = new Pine::Entity(Pine::UId::Empty());

        cameraEntity->AddComponent(new Pine::Transform());
        cameraEntity->AddComponent(new Pine::Camera());
    }

    SetupView(cameraEntity->GetComponent<Pine::Camera>(),
              lightEntity->GetComponent<Pine::Light>(),
              model->GetBoundingBoxMin(),
              model->GetBoundingBoxMax(),
              options.ViewAngle,
              static_cast<float>(options.Size.x) / static_cast<float>(options.Size.y));

    target->Bind();

    Pine::Graphics::GetGraphicsAPI()->SetViewport(Pine::Vector2i(0), options.Size);
    Pine::Graphics::GetGraphicsAPI()->ClearColor(options.Background);
    Pine::Graphics::GetGraphicsAPI()->ClearBuffers(Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::DepthBuffer);
    Pine::Graphics::GetGraphicsAPI()->SetFaceCullingEnabled(true);
    Pine::Graphics::GetGraphicsAPI()->SetFaceCullingMode(Pine::Graphics::FaceCullMode::Back);
    Pine::Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);

    Pine::Renderer3D::FrameReset();

    // Options::Ambient stands in for a fill light, since a single directional light leaves the back
    // of every subject black and this pass cannot hold a second one (AddLight puts every
    // directional in slot 0, and the point/spot slots come from instance light indices that
    // FrameReset zeroes).
    //
    // It is not a physically meaningful ambient: this pass renders the generic shader's linear HDR
    // output straight into an LDR buffer and blits, so it never gets the display transform -
    // exposure, ACES and the linear->sRGB encode all live in the post-process resolve. Values
    // therefore land roughly a 2.2 gamma too dark and anything reaching 1.0 hard-clips to white.
    // Both Options values are tuned by eye against that, and have to be retuned if this pass ever
    // resolves properly.
    Pine::Renderer3D::PrepareScene(options.Ambient);
    Pine::Renderer3D::SetCamera(cameraEntity->GetComponent<Pine::Camera>());
    Pine::Renderer3D::AddLight(lightEntity->GetComponent<Pine::Light>());
    Pine::Renderer3D::UploadLights();

    RenderSubject(asset, model);

    // The editor draws its UI into the default frame buffer, and the debug server captures after
    // that has already happened. Either way, leave the binding the way it was found.
    Pine::Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr);

    return true;
}
