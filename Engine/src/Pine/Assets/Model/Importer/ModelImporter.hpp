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
        static void ProcessMesh(Model* model, const aiMesh *mesh, const aiScene *scene);
        static void ProcessNode(Model* model, const aiNode *node, const aiScene *scene);

        static Texture2D* ImportTexture(AssetImport* context, aiMaterial* material, int type);
    public:
        static bool Import(AssetImport* importContext, Model* model);
    };

}
