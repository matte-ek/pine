#pragma once
#include "Pine/Assets/Model/Model.hpp"

class aiMesh;
struct aiScene;
struct aiMaterial;
class aiNode;

namespace Pine::Importer
{

    class ModelImporter
    {
    private:
        // 'materialIds' maps every material index in the model file onto the engine material it
        // was imported as, and holds an empty UId for the ones that weren't imported.
        static void ProcessMesh(Model* model, const aiMesh *mesh, const std::vector<UId>& materialIds);
        static void ProcessNode(Model* model, const aiNode *node, const aiScene *scene, const std::vector<UId>& materialIds);

        static Texture2D* ImportTexture(AssetImport* context, const aiScene* scene, aiMaterial* material, int type);
    public:
        static bool Import(AssetImport* importContext, Model* model);
    };

}
