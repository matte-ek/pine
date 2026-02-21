#pragma once
#include "Pine/Assets/Model/Model.hpp"

class aiMesh;
struct aiScene;
class aiNode;

namespace Pine::Importer
{

    class ModelImporter
    {
    private:
        static void ProcessMesh(Model* model, const aiMesh *mesh, const aiScene *scene);
        static void ProcessNode(Model* model, const aiNode *node, const aiScene *scene);
    public:
        static bool Import(Model* model);
    };

}
